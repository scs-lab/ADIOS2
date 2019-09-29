/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 *
 * SscWriter.h
 *
 *  Created on: Nov 1, 2018
 *      Author: Jason Wang
 */

#ifndef ADIOS2_ENGINE_SSCWRITER_H_
#define ADIOS2_ENGINE_SSCWRITER_H_

#include "adios2/core/Engine.h"
#include "adios2/toolkit/profiling/taustubs/tautimer.hpp"
#include <queue>

namespace adios2
{
namespace core
{
namespace engine
{

class SscWriter : public Engine
{

public:
    SscWriter(IO &adios, const std::string &name, const Mode mode,
              helper::Comm comm);
    ~SscWriter() = default;

    StepStatus BeginStep(
        StepMode mode,
        const float timeoutSeconds = std::numeric_limits<float>::max()) final;
    size_t CurrentStep() const final;
    void PerformPuts() final;
    void EndStep() final;
    void Flush(const int transportIndex = -1) final;

private:
    size_t m_CurrentStep = 0;
    bool m_InitialStep = true;

    struct RequestElement
    {
        Dims shape;
        Dims start;
        Dims count;
        std::string type;
    };
    using RequestMap = std::unordered_map<std::string, RequestElement>;
    std::vector<RequestMap> m_GlobalRequestMap;

    int m_WorldRank;
    int m_WorldSize;
    int m_WriterRank;
    int m_WriterSize;
    int m_ReaderSize;
    int m_WriterMasterWorldRank;
    int m_ReaderMasterWorldRank;

    void SyncRank();
    void SyncMetadata();
    void SyncRequests();

#define declare_type(T)                                                        \
    void DoPutSync(Variable<T> &, const T *) final;                            \
    void DoPutDeferred(Variable<T> &, const T *) final;
    ADIOS2_FOREACH_STDTYPE_1ARG(declare_type)
#undef declare_type

    void DoClose(const int transportIndex = -1) final;

    template <class T>
    void PutSyncCommon(Variable<T> &variable, const T *values);

    template <class T>
    void PutDeferredCommon(Variable<T> &variable, const T *values);

    int m_Verbosity = 10;
};

} // end namespace engine
} // end namespace core
} // end namespace adios2

#endif // ADIOS2_ENGINE_SSCWRITER_H_
