//
// CPC floppy media transport tool.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <memory>

#include "tools/registration.h"
#include "tools/support.h"

namespace tools {
namespace {

class disk_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "disk"; }
    std::string description() const override
    {
        return "Inspect or eject drive A. Insert standard or extended DSK "
               "images with load(format='dsk').";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .string("action", "Drive action; default status.",
                    {"status", "eject"})
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
        if (!machine().has_disk_drive())
            return mcp::tool_result::failure(
                "CPC464 has no built-in floppy subsystem");
        const std::string action = arguments["action"].as_string("status");
        if (action == "eject")
            machine().eject_disk();
        else if (action != "status")
            return mcp::tool_result::failure("unknown disk action");
        json::value result = json::value::make_object();
        result.set("inserted", json::value(machine().disk_inserted()));
        result.set("motor", json::value(machine().disk_motor_on()));
        return mcp::tool_result::of(
            machine().disk_inserted() ? "disk inserted in drive A"
                                      : "drive A is empty",
            std::move(result));
    }
};

} // namespace

void register_disk_tools(mcp::tool_registry &registry, cpc::machine &target)
{
    registry.add(std::make_unique<disk_tool>(target));
}

} // namespace tools
