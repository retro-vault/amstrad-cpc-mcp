//
// Convert CPC Gate Array colours to CTM colour or GT64 green output.
//
// The first 32 green levels are the preserved GT64 luminance values published
// by Ulrich Doewich. Renderer-only diagnostic colours use the same phosphor
// range after a weighted luminance conversion; blanking remains black.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "tools/monitor.h"

#include <array>

namespace tools {

namespace {

constexpr std::array<std::uint8_t, 32> gt64_levels = {
    144, 144, 192, 240, 48,  96,  120, 168,
    96,  240, 232, 248, 88,  104, 160, 176,
    48,  192, 184, 200, 40,  56,  112, 128,
    72,  216, 208, 224, 64,  80,  136, 152,
};

std::uint8_t diagnostic_green(cpc::rgb colour)
{
    constexpr unsigned phosphor_floor = 40;
    constexpr unsigned phosphor_range = 208;
    const unsigned luminance = 54u * colour.r + 183u * colour.g +
                               19u * colour.b;
    return static_cast<std::uint8_t>(
        phosphor_floor +
        (luminance * phosphor_range) / (255u * 256u));
}

} // namespace

std::optional<monitor_type> monitor_type_from_name(std::string_view name)
{
    if (name == "color")
        return monitor_type::color;
    if (name == "green")
        return monitor_type::green;
    return std::nullopt;
}

const char *monitor_type_name(monitor_type type)
{
    switch (type) {
    case monitor_type::color: return "color";
    case monitor_type::green: return "green";
    }
    return "color";
}

std::vector<png::colour> monitor_palette(const cpc::machine &machine,
                                         monitor_type type)
{
    const std::vector<cpc::rgb> source = machine.palette();
    std::vector<png::colour> result;
    result.reserve(source.size());

    if (type == monitor_type::color) {
        for (const cpc::rgb colour : source)
            result.push_back({colour.r, colour.g, colour.b});
        return result;
    }

    for (std::size_t index = 0; index < source.size(); ++index) {
        std::uint8_t level = 0;
        if (index < gt64_levels.size()) {
            level = gt64_levels[index];
        } else if (index + 1 != source.size()) {
            level = diagnostic_green(source[index]);
        }
        result.push_back({0, level, 0});
    }
    return result;
}

} // namespace tools
