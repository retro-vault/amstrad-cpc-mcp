//
// Bounded execution, reset and machine diagnostics MCP tools.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cstdio>
#include <memory>

#include "tools/registration.h"
#include "tools/support.h"

namespace tools {
namespace {

constexpr cpc::u64 max_tstates_per_call = 80'000'000;

std::string describe_run(const cpc::run_result &result)
{
    char buffer[256];
    std::snprintf(buffer, sizeof buffer,
                  "%s after %llu T-states (%llu instructions, %llu "
                  "frames); PC=%04X",
                  cpc::stop_reason_name(result.reason),
                  static_cast<unsigned long long>(result.tstates),
                  static_cast<unsigned long long>(result.instructions),
                  static_cast<unsigned long long>(result.frames), result.pc);
    std::string text = buffer;
    if (result.breakpoint_id >= 0)
        text += ", breakpoint " + std::to_string(result.breakpoint_id);
    return text;
}

json::value run_state(const cpc::run_result &result)
{
    json::value state = json::value::make_object();
    state.set("reason", json::value(cpc::stop_reason_name(result.reason)));
    state.set("tstates", json::value(result.tstates));
    state.set("instructions", json::value(result.instructions));
    state.set("frames", json::value(result.frames));
    state.set("pc", json::value(result.pc));
    if (result.breakpoint_id >= 0)
        state.set("breakpoint_id", json::value(result.breakpoint_id));
    return state;
}

class reset_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "reset"; }
    std::string description() const override
    {
        return "Reset the CPC chipset and Z80. RAM is preserved unless "
               "clear_memory is true.";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .boolean("clear_memory", "Clear physical RAM before reset.")
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
        const bool clear = arguments["clear_memory"].as_bool(false);
        machine().reset(clear);
        return mcp::tool_result::of(clear ? "CPC reset; RAM cleared"
                                          : "CPC reset; RAM preserved",
                                    machine_state());
    }
};

class run_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "run"; }
    std::string description() const override
    {
        return "Advance the CPC until a frame, T-state or instruction limit "
               "is reached, or a breakpoint fires. With no limit, one "
               "standard 50 Hz frame runs.";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .integer("frames", "Completed video frames.", 1, 1000)
            .integer("tstates", "4 MHz CPU T-states.", 1,
                     static_cast<std::int64_t>(max_tstates_per_call))
            .integer("instructions", "Retired Z80 instructions.", 1,
                     100'000'000)
            .boolean("stop_on_halt", "Stop when HALT is entered.")
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
        cpc::run_limits limits;
        const auto frames = read_int_in(arguments["frames"], 1, 1000);
        const auto tstates = read_int_in(arguments["tstates"], 1,
                                         max_tstates_per_call);
        const auto instructions = read_int_in(arguments["instructions"], 1,
                                               100'000'000);
        if (!arguments["frames"].is_null() && !frames)
            return mcp::tool_result::failure("invalid frames limit");
        if (!arguments["tstates"].is_null() && !tstates)
            return mcp::tool_result::failure("invalid T-state limit");
        if (!arguments["instructions"].is_null() && !instructions)
            return mcp::tool_result::failure("invalid instruction limit");
        if (frames)
            limits.max_frames = static_cast<cpc::u64>(*frames);
        if (tstates)
            limits.max_tstates = static_cast<cpc::u64>(*tstates);
        if (instructions)
            limits.max_instructions = static_cast<cpc::u64>(*instructions);
        limits.stop_on_halt = arguments["stop_on_halt"].as_bool(false);
        if (!frames && !tstates && !instructions)
            limits.max_frames = 1;
        if (!tstates)
            limits.max_tstates = max_tstates_per_call;
        const cpc::run_result result = machine().run(limits);
        return mcp::tool_result::of(describe_run(result), run_state(result));
    }
};

class run_until_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "run_until"; }
    std::string description() const override
    {
        return "Run until an instruction starts at address, a breakpoint "
               "fires, or max_tstates expires.";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .integer("address", "Target Z80 address.", 0, 0xffff)
            .integer("max_tstates", "Safety budget; default 4000000.", 1,
                     static_cast<std::int64_t>(max_tstates_per_call))
            .required({"address"})
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
        const auto address = read_int_in(arguments["address"], 0, 0xffff);
        const auto budget = read_int_in(arguments["max_tstates"], 1,
                                        max_tstates_per_call, 4'000'000);
        if (!address || !budget)
            return mcp::tool_result::failure("invalid address or budget");
        const cpc::run_result result = machine().run_until(
            static_cast<cpc::u16>(*address), static_cast<cpc::u64>(*budget));
        return mcp::tool_result::of(describe_run(result), run_state(result));
    }
};

class step_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "step"; }
    std::string description() const override
    {
        return "Execute complete Z80 instructions while every CPC chip keeps "
               "running on the same master clock.";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .integer("count", "Instructions; default 1.", 1, 1'000'000)
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
        const auto count = read_int_in(arguments["count"], 1, 1'000'000, 1);
        if (!count)
            return mcp::tool_result::failure("invalid instruction count");
        const cpc::run_result result = machine().step(*count);
        return mcp::tool_result::of(describe_run(result), run_state(result));
    }
};

class status_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "status"; }
    std::string description() const override
    {
        return "Report model, CRTC, clock, video, AY, PPI, tape, disk, ROM "
               "and execution state.";
    }
    json::value input_schema() const override
    {
        return schema_builder().build();
    }
    mcp::tool_result invoke(const json::value &) override
    {
        json::value state = machine_state();
        state.set("model", json::value(cpc::model_name(
                               machine().machine_model())));
        state.set("crtc_type", json::value(cpc::crtc_type_number(
                                   machine().fitted_crtc())));
        state.set("cpu_hz", json::value(cpc::machine::cpu_hz));
        state.set("tstates_per_frame", json::value(
                      cpc::machine::standard_tstates_per_frame));
        state.set("ram_bytes", json::value(static_cast<std::int64_t>(
                                    machine().ram_size())));
        state.set("rom_loaded", json::value(machine().rom_loaded()));
        state.set("video_mode", json::value(machine().video_mode()));
        state.set("border", json::value(machine().border_colour()));
        state.set("ay_register", json::value(
                                  machine().ay_selected_register()));
        state.set("ppi_control", json::value(machine().ppi_control()));
        state.set("disk_drive", json::value(machine().has_disk_drive()));
        state.set("disk_inserted", json::value(machine().disk_inserted()));
        state.set("disk_motor", json::value(machine().disk_motor_on()));
        const cpc::tape_status tape = machine().tape().status();
        state.set("tape_loaded", json::value(tape.loaded));
        state.set("tape_playing", json::value(tape.playing));

        std::string text = std::string(cpc::model_name(
            machine().machine_model())) + " with CRTC type " +
            std::to_string(cpc::crtc_type_number(machine().fitted_crtc())) +
            ", " + std::to_string(machine().ram_size() / 1024) +
            " KiB RAM; PC=" + hex16(machine().instruction_address()) +
            ", " + std::to_string(machine().total_tstates()) + " T-states";
        return mcp::tool_result::of(std::move(text), std::move(state));
    }
};

} // namespace

void register_machine_tools(mcp::tool_registry &registry,
                            cpc::machine &target)
{
    registry.add(std::make_unique<reset_tool>(target));
    registry.add(std::make_unique<run_tool>(target));
    registry.add(std::make_unique<run_until_tool>(target));
    registry.add(std::make_unique<step_tool>(target));
    registry.add(std::make_unique<status_tool>(target));
}

} // namespace tools
