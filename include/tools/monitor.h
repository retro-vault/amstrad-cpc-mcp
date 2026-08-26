//
// Host rendering for the colour CTM and green-phosphor GT CPC monitors.
//
// The emulated Gate Array always produces the same palette indices. This
// module models the attached monitor by converting those indices to either
// RGB output or the measured GT64 luminance levels at image-encoding time.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef TOOLS_MONITOR_H
#define TOOLS_MONITOR_H

#include <optional>
#include <string_view>
#include <vector>

#include "cpc/machine.h"
#include "png/encoder.h"

namespace tools {

// Monitor types offered by the CPC MCP image tools.
enum class monitor_type {
    color,
    green,
};

// Parse the MCP wire name `color` or `green`.
std::optional<monitor_type> monitor_type_from_name(std::string_view name);

// Return the MCP wire name for a monitor type.
const char *monitor_type_name(monitor_type type);

// Build the PNG palette seen through the selected monitor.
std::vector<png::colour> monitor_palette(const cpc::machine &machine,
                                         monitor_type type);

} // namespace tools

#endif // TOOLS_MONITOR_H
