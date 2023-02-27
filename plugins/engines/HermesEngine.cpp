//
// Created by jaime on 2/27/2023.
//

#include "HermesEngine.h"

#include <adios2/engine/inline/InlineWriter.h>

#include <hermes.h> //TODO: Set this up

#include <iostream>
#include <sstream>

namespace hermes_plugin
{

struct HermesEngine::EngineImpl
{
    adios2::core::IO *Io;
    adios2::core::Engine *Writer;

    std::string ScriptFileName;
    std::string JSONFileName;

    int Rank;

    EngineImpl(adios2::core::ADIOS &adios)
    {
        this->Io = &adios.DeclareIO("InlinePluginIO");
        this->Io->SetEngine("inline");
        this->Writer = &Io->Open("write", adios2::Mode::Write);
    }

    void CatalystConfig()
    {
        std::cout << "\tCatalyst Library Version: " << CATALYST_VERSION << "\n";
        std::cout << "\tCatalyst ABI Version: " << CATALYST_ABI_VERSION << "\n";

        conduit_cpp::Node node;
        catalyst_about(conduit_cpp::c_node(&node));
        auto implementation = node.has_path("catalyst/implementation")
                                  ? node["catalyst/implementation"].as_string()
                                  : std::string("stub");
        std::cout << "\tImplementation: " << implementation << "\n\n";
    }

    void CatalystInit()
    {
        conduit_cpp::Node node;
        node["catalyst/scripts/script/filename"].set(this->ScriptFileName);

        // options to set up the fides reader in paraview
        std::ostringstream address;
        address << &Io;

        node["catalyst/fides/json_file"].set(this->JSONFileName);
        node["catalyst/fides/data_source_io/source"].set(std::string("source"));
        node["catalyst/fides/data_source_io/address"].set(address.str());
        node["catalyst/fides/data_source_path/source"].set(
            std::string("source"));
        node["catalyst/fides/data_source_path/path"].set(
            std::string("DataReader"));
        catalyst_initialize(conduit_cpp::c_node(&node));

        if (this->Rank == 0)
        {
            this->CatalystConfig();
        }
    }

    void CatalystExecute()
    {
        auto timestep = this->Writer->CurrentStep();
        conduit_cpp::Node node;
        node["catalyst/state/timestep"].set(timestep);
        // catalyst requires the next one, but when using Fides as the reader
        // for Catalyst, it will grab the time from the correct adios variable
        // if it is specified in the data model
        node["catalyst/state/time"].set(timestep);
        node["catalyst/channels/fides/type"].set(std::string("fides"));

        // options to set up the fides reader in paraview
        std::ostringstream address;
        address << &Io;

        node["catalyst/fides/json_file"].set(this->JSONFileName);
        node["catalyst/fides/data_source_io/source"].set(std::string("source"));
        node["catalyst/fides/data_source_io/address"].set(address.str());
        node["catalyst/fides/data_source_path/source"].set(
            std::string("source"));
        node["catalyst/fides/data_source_path/path"].set(
            std::string("DataReader"));

        // catalyst requires the data node on a channel, but we don't actually
        // need it when using fides, so just create a dummy object to pass
        // the validation in catalyst
        conduit_cpp::Node dummy;
        dummy["dummy"].set(0);
        node["catalyst/channels/fides/data"].set(dummy);

        catalyst_execute(conduit_cpp::c_node(&node));
    }
};

HermesEngine::HermesEngine(adios2::core::IO &io,
                           const std::string &name,
                           adios2::helper::Comm comm)
: adios2::plugin::PluginEngineInterface(io, name, adios2::Mode::Write,
                                        comm.Duplicate()),
  Impl(new EngineImpl(io.m_ADIOS)){

}

HermesEngine::~HermesEngine(){

}

adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                           const float timeoutSeconds){

}

size_t HermesEngine::CurrentStep() const{
}

void HermesEngine::EndStep(){
}

void HermesEngine::PerformPuts() {
}


void HermesEngine::DoClose(const int transportIndex){
}

} // end namespace hermes_plugin

