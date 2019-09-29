/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 *
 * SscReader.cpp
 *
 *  Created on: Nov 1, 2018
 *      Author: Jason Wang
 */

#include "SscReader.tcc"
#include "nlohmann/json.hpp"
#include "adios2/helper/adiosFunctions.h"

namespace adios2
{
namespace core
{
namespace engine
{

SscReader::SscReader(IO &io, const std::string &name, const Mode mode,
                     helper::Comm comm)
: Engine("SscReader", io, name, mode, std::move(comm))
{
    TAU_SCOPED_TIMER_FUNC();
    MPI_Comm_rank(MPI_COMM_WORLD, &m_WorldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &m_WorldSize);
    m_ReaderRank = m_Comm.Rank();
    m_ReaderSize = m_Comm.Size();
    m_WriterSize = m_WorldSize - m_ReaderSize;

    SyncRank();

    SyncMetadata();
}

SscReader::~SscReader() { TAU_SCOPED_TIMER_FUNC(); }

StepStatus SscReader::BeginStep(const StepMode stepMode,
                                const float timeoutSeconds)
{
    TAU_SCOPED_TIMER_FUNC();

    if(m_Verbosity >=5)
    {
        std::cout << "SscReader::BeginStep, World Rank " << m_WorldRank << ", Reader Rank " << m_ReaderRank << std::endl;
    }

    if(m_InitialStep)
    {
        m_InitialStep = false;
        SyncRequests();
    }
    else{
        ++m_CurrentStep;
    }
    return StepStatus::OK;
}

void SscReader::PerformGets() { TAU_SCOPED_TIMER_FUNC(); }

size_t SscReader::CurrentStep() const
{
    TAU_SCOPED_TIMER_FUNC();
    return m_CurrentStep;
}

void SscReader::EndStep()
{
    TAU_SCOPED_TIMER_FUNC();
    if(m_Verbosity >=5)
    {
        std::cout << "SscReader::EndStep, World Rank " << m_WorldRank << ", Reader Rank " << m_ReaderRank << std::endl;
    }
}

// PRIVATE

void SscReader::SyncRank()
{
    int readerMasterWorldRank = 0;
    int writerMasterWorldRank = 0;
    if(m_ReaderRank == 0)
    {
        readerMasterWorldRank = m_WorldRank;
    }
    MPI_Allreduce(&readerMasterWorldRank, &m_ReaderMasterWorldRank, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&writerMasterWorldRank, &m_WriterMasterWorldRank, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
}

void SscReader::SyncMetadata()
{
    if(m_Verbosity >=5)
    {
        std::cout << "SscReader::SyncMetadata, World Rank " << m_WorldRank << ", Reader Rank " << m_ReaderRank << std::endl;
    }

    //sync
    size_t metadataSize = 0;
    MPI_Bcast(&metadataSize, 1, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);
    std::vector<char> metadataJsonCharVector(metadataSize);

    MPI_Win win;
    MPI_Win_create(NULL, 0, sizeof(char), MPI_INFO_NULL, MPI_COMM_WORLD, &win);
    MPI_Win_fence(0, win);
    MPI_Get(metadataJsonCharVector.data(), metadataJsonCharVector.size(), MPI_CHAR, 0,0,metadataJsonCharVector.size(), MPI_CHAR, win);
    MPI_Win_fence(0, win);
    MPI_Win_free(&win);

    //deserialize
    nlohmann::json j = nlohmann::json::parse(metadataJsonCharVector);

    if(m_Verbosity >=5)
    {
        std::cout << "SscReader::DeserializeMetadata obtained metadata: ";
        std::cout << j.dump(4) << std::endl;
    }

    auto itAV = j.find("A");
    if(itAV != j.end())
    {
        // TODO: Add attributes
    }
    itAV = j.find("V");
    if(itAV != j.end())
    {
        std::cout << itAV->dump(4) << std::endl;
        for(auto itVar = itAV->begin(); itVar != itAV->end(); ++itVar)
        {
            std::string shapeStr;
            std::string type;
            auto itST = itVar.value().find("S");
            if(itST != itVar.value().end())
            {
                shapeStr = *itST;
            }
            else{
                throw(std::runtime_error("metadata corrupted"));
            }
            itST = itVar.value().find("T");
            if(itST != itVar.value().end())
            {
                type = *itST;
            }
            else{
                throw(std::runtime_error("metadata corrupted"));
            }
            Dims shape;
            try
            {
                shape = helper::StringToDims(shapeStr);
            }
            catch(...)
            {
                throw(std::runtime_error("metadata corrupted"));
            }
            Dims start = Dims(shape.size(), 0);
            if(type.empty())
            {
                throw(std::runtime_error("unknown data type"));
            }
#define declare_type(T)                                                        \
            else if (type == helper::GetType<T>())                                   \
            {                                                                          \
                m_IO.DefineVariable<T>(itVar.key(), shape, start, shape);\
                m_LocalRequestMap[itVar.key()].shape = shape;\
                m_LocalRequestMap[itVar.key()].type = type;\
            }
            ADIOS2_FOREACH_STDTYPE_1ARG(declare_type)
#undef declare_type
            else {
                throw(std::runtime_error("unknown data type"));
            }
        }
    }
}

void SscReader::SyncRequests()
{
    if(m_Verbosity >=5)
    {
        std::cout << "SscReader::SyncRequests, World Rank " << m_WorldRank << ", Reader Rank " << m_ReaderRank << std::endl;
    }

    //serialize
    for(auto &var : m_LocalRequestMap)
    {
        if(var.second.type.empty())
        {
            throw(std::runtime_error("unknown data type"));
        }
#define declare_type(T)                                                        \
        else if (var.second.type == helper::GetType<T>())                                   \
        {                                                                          \
            auto v = m_IO.InquireVariable<T>(var.first);\
            if(v) {\
                var.second.count = v->m_Count;\
                var.second.start = v->m_Start;\
            }\
        }
        ADIOS2_FOREACH_STDTYPE_1ARG(declare_type)
#undef declare_type
        else {
            throw(std::runtime_error("unknown data type"));
        }
    }

    nlohmann::json j;
    for(auto &var : m_LocalRequestMap)
    {
        j[var.first]["T"] = var.second.type;
        j[var.first]["S"] = var.second.shape;
        j[var.first]["O"] = var.second.start;
        j[var.first]["C"] = var.second.count;
    }

    std::string localRequestJsonString = j.dump();

    // sync
    size_t localSize = localRequestJsonString.size();
    size_t maxLocalSize;
    m_Comm.Allreduce(&localSize, &maxLocalSize, 1, MPI_MAX);
    std::vector<char> element(maxLocalSize, '\0');
    std::memcpy(element.data(), localRequestJsonString.c_str(), localRequestJsonString.size());
    std::vector<char> array(maxLocalSize * m_ReaderSize);
    m_Comm.GatherArrays(element.data(), maxLocalSize, array.data(), 0);

    std::string globalRequestJsonString;
    if(m_ReaderRank == 0)
    {
        nlohmann::json allRequests;
        for(size_t i=0; i<m_ReaderSize; ++i)
        {
            auto request = nlohmann::json::parse(array.begin()+i*maxLocalSize, array.begin()+(i+1)*maxLocalSize);
            allRequests[i] = request;
        }
        globalRequestJsonString = allRequests.dump();
    }

    size_t globalRequestSizeSrc = globalRequestJsonString.size();
    size_t globalRequestSizeDst = 0;
    MPI_Allreduce(&globalRequestSizeSrc, &globalRequestSizeDst, 1, MPI_UNSIGNED_LONG_LONG, MPI_MAX, MPI_COMM_WORLD);

    if(globalRequestJsonString.size() < globalRequestSizeDst)
    {
        globalRequestJsonString.resize(globalRequestSizeDst);
    }

    std::vector<char> globalRequestCharVec(globalRequestJsonString.size());
    std::memcpy(globalRequestCharVec.data(), globalRequestJsonString.data(), globalRequestJsonString.size() );

    MPI_Win win;
    MPI_Win_create(globalRequestCharVec.data(), globalRequestCharVec.size(), sizeof(char), MPI_INFO_NULL, MPI_COMM_WORLD, &win);
    MPI_Win_fence(0, win);
    if(m_ReaderRank !=0)
    {
        MPI_Get(globalRequestCharVec.data(), globalRequestCharVec.size(), MPI_CHAR, m_ReaderMasterWorldRank,0,globalRequestCharVec.size(), MPI_CHAR, win);
    }
    MPI_Win_fence(0, win);
    MPI_Win_free(&win);
}

#define declare_type(T)                                                        \
    void SscReader::DoGetSync(Variable<T> &variable, T *data)                  \
    {                                                                          \
        GetSyncCommon(variable, data);                                         \
    }                                                                          \
    void SscReader::DoGetDeferred(Variable<T> &variable, T *data)              \
    {                                                                          \
        GetDeferredCommon(variable, data);                                     \
    }                                                                          \
    std::map<size_t, std::vector<typename Variable<T>::Info>>                  \
    SscReader::DoAllStepsBlocksInfo(const Variable<T> &variable) const         \
    {                                                                          \
        return AllStepsBlocksInfoCommon(variable);                             \
    }                                                                          \
    std::vector<typename Variable<T>::Info> SscReader::DoBlocksInfo(           \
        const Variable<T> &variable, const size_t step) const                  \
    {                                                                          \
        return BlocksInfoCommon(variable, step);                               \
    }
ADIOS2_FOREACH_STDTYPE_1ARG(declare_type)
#undef declare_type

void SscReader::DoClose(const int transportIndex)
{
    TAU_SCOPED_TIMER_FUNC();
    if(m_Verbosity >=5)
    {
        std::cout << "SscReader::DoClose, World Rank " << m_WorldRank << ", Reader Rank " << m_ReaderRank << std::endl;
    }
}

} // end namespace engine
} // end namespace core
} // end namespace adios2
