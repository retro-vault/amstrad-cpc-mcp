//
// CTM colour and GT64 green-monitor palette rendering.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <string>

#include "cpc/machine.h"
#include "tools/monitor.h"
#include "test_support.h"

namespace {

void test_monitor_names()
{
    test::section("CPC monitor names");
    test::check(tools::monitor_type_from_name("color") ==
                    tools::monitor_type::color,
                "color selects the CTM monitor");
    test::check(tools::monitor_type_from_name("green") ==
                    tools::monitor_type::green,
                "green selects the GT64 monitor");
    test::check(!tools::monitor_type_from_name("amber"),
                "unsupported monitor names are rejected");
}

void test_gt64_palette()
{
    test::section("GT64 luminance palette");
    cpc::machine machine;
    const auto color = tools::monitor_palette(
        machine, tools::monitor_type::color);
    const auto green = tools::monitor_palette(
        machine, tools::monitor_type::green);

    test::check_eq(green.size(), color.size(),
                   "monitor palettes have identical index counts");
    test::check(color[11].r != 0 || color[11].b != 0,
                "color monitor retains RGB channels");
    test::check_eq(green[20].g, 40,
                   "hardware black has the GT64 phosphor floor");
    test::check_eq(green[11].g, 248,
                   "bright white has the GT64 peak level");
    test::check_eq(green[3].g, 240,
                   "pastel yellow has its measured GT64 level");

    bool only_green = true;
    for (std::size_t index = 0; index < 32; ++index) {
        only_green = only_green && green[index].r == 0 &&
                     green[index].b == 0;
    }
    test::check(only_green, "every hardware ink is green monochrome");
    test::check_eq(green.back().g, 0,
                   "monitor blanking remains black");
}

} // namespace

int main()
{
    test_monitor_names();
    test_gt64_palette();
    return test::summary("monitor");
}
