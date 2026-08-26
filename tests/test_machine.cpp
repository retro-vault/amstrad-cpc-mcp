//
// CPC model, memory, port, timing, video and peripheral conformance.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <array>
#include <vector>

#include "cpc/machine.h"
#include "test_support.h"

namespace {

void test_models()
{
    test::section("three classic models");
    cpc::machine c464({cpc::model::cpc464, cpc::crtc_type::type_0});
    cpc::machine c664({cpc::model::cpc664, cpc::crtc_type::type_1});
    cpc::machine c6128({cpc::model::cpc6128, cpc::crtc_type::type_2});
    test::check_eq(c464.ram_size(), 64 * 1024, "CPC464 has 64 KiB RAM");
    test::check_eq(c664.ram_size(), 64 * 1024, "CPC664 has 64 KiB RAM");
    test::check_eq(c6128.ram_size(), 128 * 1024,
                   "CPC6128 has 128 KiB RAM");
    test::check(!c464.has_disk_drive(), "CPC464 has no built-in FDC");
    test::check(c664.has_disk_drive(), "CPC664 has a built-in FDC");
    test::check(c6128.has_disk_drive(), "CPC6128 has a built-in FDC");
    test::check_eq(cpc::crtc_type_number(c6128.fitted_crtc()), 2,
                   "selected CRTC variant is retained");
}

void test_rom_and_ram_banking()
{
    test::section("ROM overlays and 6128 RAM banking");
    cpc::machine target;
    std::array<cpc::u8, 0x4000> os{};
    std::array<cpc::u8, 0x4000> basic{};
    std::array<cpc::u8, 0x4000> amsdos{};
    os.fill(0x11);
    basic.fill(0x22);
    amsdos.fill(0x33);
    std::string error;
    test::check(target.load_roms(os, basic, amsdos, error),
                "a complete 6128 firmware set loads");
    test::check_eq(target.read_memory(0), 0x11, "lower ROM is visible");
    test::check_eq(target.read_memory(0xc000), 0x22,
                   "BASIC upper ROM is visible");
    target.write_memory(0, 0x5a);
    target.write_port(0x7f00, 0x8c);
    test::check_eq(target.read_memory(0), 0x5a,
                   "writes under ROM reached bank-zero RAM");

    target.write_memory(0x4000, 0xa1);
    target.write_port(0x7f00, 0xc2);
    test::check_eq(target.read_memory(0x4000), 0,
                   "RAM configuration 2 exposes expansion bank 5");
    target.write_memory(0x4000, 0xb2);
    target.write_port(0x7f00, 0xc0);
    test::check_eq(target.read_memory(0x4000), 0xa1,
                   "RAM configuration 0 restores bank 1");
    target.write_port(0x7f00, 0xc2);
    test::check_eq(target.read_memory(0x4000), 0xb2,
                   "expansion-bank data persists");

    target.write_port(0xdf00, 7);
    target.write_port(0x7f00, 0x84);
    test::check_eq(target.read_memory(0xc000), 0x33,
                   "upper ROM slot 7 selects AMSDOS");
}

void test_instruction_timing()
{
    test::section("CPC-rounded Z80 timing");
    cpc::machine target;
    target.write_port(0x7f00, 0x8c);
    target.write_memory(0x8000,
                        std::vector<cpc::u8>{0x00, 0x01, 0x34, 0x12,
                                             0x3a, 0x00, 0x90, 0x00});
    target.write_memory(0x9000, 0x6b);
    target.jump(0x8000);
    target.run_tstates(1);
    test::check_eq(target.step(1).tstates, 4, "NOP takes one 4-T slot");
    test::check_eq(target.step(1).tstates, 12,
                   "LD BC,nn has three rounded M-cycles");
    test::check_eq(target.step(1).tstates, 16,
                   "LD A,(nn) has four rounded M-cycles");
    test::check_eq(target.registers().af >> 8, 0x6b,
                   "timed memory read returned its byte");
}

void test_video_sound_and_ports()
{
    test::section("screen, Gate Array, CRTC, PPI and AY");
    cpc::machine target;
    test::check_eq(target.screen_pixels().size(),
                   cpc::machine::screen_width * cpc::machine::screen_height,
                   "overscan raster has the documented geometry");
    test::check_eq(target.display_pixels().size(),
                   cpc::machine::display_width *
                       cpc::machine::display_height,
                   "active display crop has the documented geometry");
    test::check(target.palette().size() >= 32,
                "renderer exposes every Gate Array hardware colour");

    target.write_port(0x7f00, 0x10);
    target.write_port(0x7f00, 0x4b);
    test::check_eq(target.border_colour(), 11,
                   "Gate Array border pen write is decoded");
    target.write_port(0x7f00, 0x82);
    target.run_tstates(256);
    test::check_eq(target.video_mode(), 2,
                   "video mode changes on the following HSYNC");

    target.write_port(0xbc00, 12);
    target.write_port(0xbd00, 0x2f);
    test::check_eq(target.read_port(0xbf00), 0x2f,
                   "CRTC register select/data ports overlap correctly");
    test::check_eq(target.ppi_control(), 0x9b,
                   "8255 reset control word is mode-zero input");

    target.set_ay_register(1, 0xff);
    target.set_ay_register(6, 0xff);
    target.set_ay_register(8, 0xff);
    test::check_eq(target.ay_register(1), 0x0f,
                   "AY coarse tone period is four bits");
    test::check_eq(target.ay_register(6), 0x1f,
                   "AY noise period is five bits");
    test::check_eq(target.ay_register(8), 0x1f,
                   "AY amplitude register is five bits");

    const auto ports = target.port_map();
    test::check(ports.size() >= 8,
                "6128 reports every internal partial decoder");
}

void test_beam_synchronous_border()
{
    test::section("beam-synchronous raster writes");

    cpc::machine target;
    target.run_frames(2);
    target.write_port(0x7f00, 0x10);
    target.write_port(0x7f00, 0x54);
    target.run_tstates(cpc::machine::standard_tstates_per_frame / 2);
    target.write_port(0x7f00, 0x4b);
    target.run_tstates(cpc::machine::standard_tstates_per_frame / 2);

    const std::vector<cpc::u8> raster = target.screen_pixels();
    const auto old_colour = std::count(raster.begin(), raster.end(), 20);
    const auto new_colour = std::count(raster.begin(), raster.end(), 11);
    test::check(old_colour > 1000,
                "the first half retains pixels from the old border ink");
    test::check(new_colour > 1000,
                "the second half contains pixels from the new border ink");
}

void test_floppy_validation()
{
    test::section("floppy model and validation");
    std::string error;
    cpc::machine c464({cpc::model::cpc464, cpc::crtc_type::type_0});
    cpc::machine c664({cpc::model::cpc664, cpc::crtc_type::type_0});
    const std::vector<cpc::u8> junk = {1, 2, 3};
    test::check(!c464.insert_disk(junk, error),
                "CPC464 refuses a built-in drive operation");
    test::check_contains(error, "no built-in", "464 error names the reason");
    test::check(!c664.insert_disk(junk, error),
                "CPC664 validates DSK structure");
    test::check_contains(error, "valid", "bad DSK error is explicit");

    c664.write_port(0xfb7f, 0x03);
    test::check((c664.read_port(0xfb7e) & 0x10) != 0,
                "an incomplete FDC command leaves the controller busy");
    c664.reset();
    test::check_eq(c664.read_port(0xfb7e), 0x80,
                   "machine reset returns the FDC to idle/RQM");
}

} // namespace

int main()
{
    test_models();
    test_rom_and_ram_banking();
    test_instruction_timing();
    test_video_sound_and_ports();
    test_beam_synchronous_border();
    test_floppy_validation();
    return test::summary("machine");
}
