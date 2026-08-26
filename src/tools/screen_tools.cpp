//
// Rendered CPC raster tools.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "png/encoder.h"
#include "tools/monitor.h"
#include "tools/registration.h"
#include "tools/support.h"

namespace tools {
namespace {

class screen_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "screen"; }
    std::string description() const override
    {
        return "Return the cycle-rendered CPC raster as indexed PNG. Include "
               "the overscan/border (768x272) or crop to 640x200, rendered "
               "through a color or green CPC monitor.";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .boolean("include_border", "Include overscan; default true.")
            .integer("scale", "Nearest-neighbour scale; default 1.", 1, 4)
            .string("monitor", "CPC monitor; default color.",
                    {"color", "green"})
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
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
        const std::vector<std::uint8_t> encoded = png::encode_indexed(
            pixels, width, height, palette, static_cast<int>(*scale));
        if (encoded.empty())
            return mcp::tool_result::failure("could not encode CPC raster");

        json::value state = machine_state();
        state.set("width", json::value(width * *scale));
        state.set("height", json::value(height * *scale));
        state.set("video_mode", json::value(machine().video_mode()));
        state.set("border", json::value(machine().border_colour()));
        state.set("monitor", json::value(monitor_type_name(*monitor)));
        const std::string_view bytes(
            reinterpret_cast<const char *>(encoded.data()), encoded.size());
        mcp::tool_result result;
        result.content.push_back(mcp::content_block::text(
            "CPC frame " + std::to_string(machine().frame_number()) +
            ": " + std::to_string(width) + "x" +
            std::to_string(height)));
        result.content.push_back(mcp::content_block::image(
            mcp::base64_encode(bytes), "image/png"));
        result.structured = std::move(state);
        return result;
    }
};

class screen_text_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "screen_text"; }
    std::string description() const override
    {
        return "Return a luminance preview of the 640x200 CPC display as "
               "80x25 text. This is a pixel preview, not firmware-font OCR.";
    }
    json::value input_schema() const override
    {
        return schema_builder().build();
    }
    mcp::tool_result invoke(const json::value &) override
    {
        constexpr std::string_view levels = " .:-=+*#%@";
        const std::vector<cpc::u8> pixels = machine().display_pixels();
        const std::vector<cpc::rgb> palette = machine().palette();
        std::string text;
        for (int cell_y = 0; cell_y < 25; ++cell_y) {
            for (int cell_x = 0; cell_x < 80; ++cell_x) {
                unsigned luminance = 0;
                for (int y = 0; y < 8; ++y) {
                    for (int x = 0; x < 8; ++x) {
                        const cpc::u8 index = pixels[
                            (cell_y * 8 + y) * cpc::machine::display_width +
                            cell_x * 8 + x];
                        const cpc::rgb colour = palette[std::min<std::size_t>(
                            index, palette.size() - 1)];
                        luminance += 54u * colour.r + 183u * colour.g +
                                     19u * colour.b;
                    }
                }
                const unsigned average = luminance / (64u * 256u);
                text += levels[average * (levels.size() - 1) / 255u];
            }
            if (cell_y != 24)
                text += '\n';
        }
        return mcp::tool_result::text(std::move(text));
    }
};

} // namespace

void register_screen_tools(mcp::tool_registry &registry,
                           cpc::machine &target)
{
    registry.add(std::make_unique<screen_tool>(target));
    registry.add(std::make_unique<screen_text_tool>(target));
}

void register_all_tools(mcp::tool_registry &registry,
                        cpc::machine &target,
                        std::string cpm_disk)
{
    register_load_tool(registry, target);
    register_tape_tools(registry, target);
    register_disk_tools(registry, target);
    register_cpm_tool(registry, target, std::move(cpm_disk));
    register_machine_tools(registry, target);
    register_memory_tools(registry, target);
    register_cpu_tools(registry, target);
    register_io_tools(registry, target);
    register_screen_tools(registry, target);
    register_capture_tools(registry, target);
    register_audio_tools(registry, target);
}

} // namespace tools
