#pragma once

#include "crossa/compiler/ir/Program.h"
#include "crossa/network/NetworkConfiguration.h"
#include "crossa/network/json/JsonValue.h"
#include "crossa/runtime/scheduler/SchedulerOptions.h"

namespace crossa::runtime {

// Owns validated networking and scheduler settings loaded from Crossa IR.
// load() combines at most one declarative config block across source programs.
class RuntimeConfiguration final {
public:
    // Loads defaults and applies the optional main or sibling config block.
    [[nodiscard]] static RuntimeConfiguration load(
        const compiler::ir::Program& program,
        const compiler::ir::Program* configurationProgram,
        const network::json::JsonValue* runtimeOverrides = nullptr
    );

    // Returns immutable native networking configuration.
    [[nodiscard]] const network::NetworkConfiguration&
    getNetworkConfiguration() const noexcept;

    // Returns immutable bounded scheduler configuration.
    [[nodiscard]] const scheduler::SchedulerOptions&
    getSchedulerOptions() const noexcept;

private:
    friend class RuntimeConfigurationLoader;

    // Creates one complete runtime configuration.
    RuntimeConfiguration(
        network::NetworkConfiguration networkConfiguration,
        scheduler::SchedulerOptions schedulerOptions
    );

    network::NetworkConfiguration networkConfiguration_;
    scheduler::SchedulerOptions schedulerOptions_;
};

}
