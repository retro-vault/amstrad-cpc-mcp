//
// Persistent CPC PNG screenshot tool.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <fstream>
#include <memory>

#include "png/encoder.h"
#include "tools/monitor.h"
#include "tools/registration.h"
#include "tools/support.h"

namespace tools {
namespace {

class screenshot_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "screenshot"; }
    std::string description() const override
    {
        return "Save the current cycle-rendered CPC raster to a host PNG "
               "file through a color or green CPC monitor, with optional "
               "overscan and integer scaling.";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .string("path", "Destination PNG path.")
            .boolean("include_border", "Include 768x272 overscan; default true.")
            .integer("scale", "Nearest-neighbour scale; default 1.", 1, 4)
            .string("monitor", "CPC monitor; default color.",
                    {"color", "green"})
            .required({"path"})
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
        const std::string path = arguments["path"].as_string();
        if (path.empty())
            return mcp::tool_result::failure("path is required");
        const bool border = arguments["include_border"].as_bool(true);
        const auto scale = read_int_in(arguments["scale"], 1, 4, 1);
        if (!scale)
            return mcp::tool_result::failure("scale must be 1..4");
        const auto monitor = monitor_type_from_name(
            arguments["monitor"].as_string("color"));
        if (!monitor)
            return mcp::tool_result::failure(
                "monitor must be 'color' or 'green'");
        const int width = border ? cpc::machine::screen_width
                                 : cpc::machine::display_width;
        const int height = border ? cpc::machine::screen_height
                                  : cpc::machine::display_height;
        const std::vector<cpc::u8> pixels = border
                                                ? machine().screen_pixels()
                                                : machine().display_pixels();
        const std::vector<png::colour> palette =
            monitor_palette(machine(), *monitor);
        const std::vector<std::uint8_t> data = png::encode_indexed(
            pixels, width, height, palette, static_cast<int>(*scale));
        if (data.empty())
            return mcp::tool_result::failure("could not encode screenshot");
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
            return mcp::tool_result::failure("cannot create '" + path + "'");
        file.write(reinterpret_cast<const char *>(data.data()),
                   static_cast<std::streamsize>(data.size()));
        if (!file)
            return mcp::tool_result::failure("cannot write '" + path + "'");
        json::value result = json::value::make_object();
        result.set("path", json::value(path));
        result.set("width", json::value(width * *scale));
        result.set("height", json::value(height * *scale));
        result.set("monitor", json::value(monitor_type_name(*monitor)));
        result.set("bytes", json::value(static_cast<std::int64_t>(
                                data.size())));
        return mcp::tool_result::of("saved CPC screenshot to '" + path + "'",
                                    std::move(result));
    }
};

} // namespace

void register_capture_tools(mcp::tool_registry &registry,
                            cpc::machine &target)
{
    registry.add(std::make_unique<screenshot_tool>(target));
}

} // namespace tools
