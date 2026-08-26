//
// Boot the bundled CP/M 2.2 system disk on a stock CPC6128.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <memory>
#include <string_view>
#include <utility>

#include "tools/registration.h"
#include "tools/support.h"

namespace tools {
namespace {

constexpr cpc::u64 firmware_boot_frames = 100;
constexpr cpc::u64 key_hold_frames = 2;
constexpr cpc::u64 key_gap_frames = 1;
constexpr std::string_view cpm_command = "|cpm\n";

bool run_exact_frames(cpc::machine &machine, cpc::u64 frames)
{
    return machine.run_frames(frames).frames == frames;
}

bool type_cpm_command(cpc::machine &machine)
{
    for (const char character : cpm_command) {
        const std::string key = character == '\n'
                                    ? "ENTER"
                                    : std::string(1, character);
        if (!machine.key_down(key)) {
            machine.release_keys();
            return false;
        }
        if (!run_exact_frames(machine, key_hold_frames)) {
            machine.release_keys();
            return false;
        }
        machine.release_keys();
        if (!run_exact_frames(machine, key_gap_frames))
            return false;
    }
    return true;
}

class cpm_tool final : public machine_tool {
public:
    cpm_tool(cpc::machine &target, std::string default_disk)
        : machine_tool(target), default_disk_(std::move(default_disk))
    {
    }

    std::string name() const override { return "cpm"; }

    std::string description() const override
    {
        return "Reset a CPC6128, insert the bundled CP/M 2.2 system disk, "
               "type |CPM through the emulated keyboard and run its "
               "startup sequence.";
    }

    json::value input_schema() const override
    {
        return schema_builder()
            .string("path", "Optional CP/M 2.2-compatible DSK override.")
            .integer("frames", "Frames after |CPM; default 1000.",
                     1, 5000)
            .build();
    }

    mcp::tool_result invoke(const json::value &arguments) override
    {
        if (machine().machine_model() != cpc::model::cpc6128)
            return mcp::tool_result::failure(
                "CP/M mode requires a CPC6128");
        if (!machine().rom_loaded())
            return mcp::tool_result::failure(
                "CP/M mode requires stock CPC6128 and AMSDOS firmware");

        const auto frames = read_int_in(arguments["frames"], 1, 5000, 1000);
        if (!frames)
            return mcp::tool_result::failure("frames must be 1..5000");
        const std::string path = arguments["path"].as_string(default_disk_);
        if (path.empty())
            return mcp::tool_result::failure("CP/M disk path is empty");

        std::string error;
        const auto disk = cpc::read_file(path, error);
        if (!disk)
            return mcp::tool_result::failure(error);

        machine().reset(true);
        if (!machine().insert_disk(*disk, error))
            return mcp::tool_result::failure(error);
        if (!run_exact_frames(machine(), firmware_boot_frames))
            return mcp::tool_result::failure(
                "CP/M firmware boot was interrupted by a breakpoint");
        if (!type_cpm_command(machine()))
            return mcp::tool_result::failure(
                "CP/M command entry was interrupted or rejected");
        if (!run_exact_frames(machine(), static_cast<cpc::u64>(*frames)))
            return mcp::tool_result::failure(
                "CP/M startup was interrupted by a breakpoint");

        json::value state = machine_state();
        state.set("mode", json::value("cpm"));
        state.set("version", json::value("2.2"));
        state.set("disk", json::value(path));
        state.set("disk_bytes", json::value(
                      static_cast<std::int64_t>(disk->size())));
        state.set("firmware_frames", json::value(
                      static_cast<std::int64_t>(firmware_boot_frames)));
        state.set("startup_frames", json::value(*frames));
        state.set("disk_inserted", json::value(machine().disk_inserted()));
        state.set("disk_motor", json::value(machine().disk_motor_on()));
        return mcp::tool_result::of(
            "ran the CP/M 2.2 boot sequence from '" + path + "'",
            std::move(state));
    }

private:
    std::string default_disk_;
};

} // namespace

void register_cpm_tool(mcp::tool_registry &registry, cpc::machine &target,
                       std::string default_disk)
{
    registry.add(std::make_unique<cpm_tool>(target,
                                            std::move(default_disk)));
}

} // namespace tools
