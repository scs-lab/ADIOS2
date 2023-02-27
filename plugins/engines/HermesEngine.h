//
// Created by jaime on 2/27/2023.
//

#ifndef ADIOS2_PLUGINS_ENGINES_HERMESENGINE_H_
#define ADIOS2_PLUGINS_ENGINES_HERMESENGINE_H_

#include "adios2/common/ADIOSMacros.h"
#include "adios2/common/ADIOSTypes.h"
#include "adios2/core/IO.h"
#include "adios2/engine/plugin/PluginEngineInterface.h"
#include "adios2/helper/adiosComm.h"
#include "adios2/helper/adiosType.h"

#include <memory>

namespace hermes_plugin{

class HermesEngine : public adios2::plugin::PluginEngineInterface
{

public:
    HermesEngine(adios2::core::IO &adios, const std::string &name,
                        adios2::helper::Comm comm);

    ~HermesEngine() override;

    adios2::StepStatus BeginStep(adios2::StepMode mode,
                                 const float timeoutSeconds = -1.0) override;

    void EndStep() override;

    size_t CurrentStep() const override;

    void PerformPuts() override;

protected:
#define declare_type(T)                                                        \
    void DoPutSync(adios2::core::Variable<T> &, const T *) override;           \
    void DoPutDeferred(adios2::core::Variable<T> &, const T *) override;
    ADIOS2_FOREACH_STDTYPE_1ARG(declare_type)
#undef declare_type

    void DoClose(const int transportIndex = -1) override;

private:
    struct EngineImpl;
    std::unique_ptr<EngineImpl> Impl;
};

} // end namespace hermes_plugin

#endif // ADIOS2_PLUGINS_ENGINES_HERMESENGINE_H_
