//
// The complete classic Amstrad CPC, assembled and clocked.
//
// This facade owns the Z80, Gate Array, 6845 CRTC, 8255 PPI,
// AY-3-8912, keyboard, cassette transport and optional uPD765A floppy
// subsystem. The hardware is advanced one 4 MHz T-state at a time and the
// Gate Array derives its 1 MHz CRTC and sound clocks from that master step.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef CPC_MACHINE_H
#define CPC_MACHINE_H

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "cpc/breakpoints.h"
#include "cpc/tape.h"
#include "cpc/types.h"

namespace cpc {

// Classic CPC models supported by the emulator.
enum class model : u8 {
    cpc464,
    cpc664,
    cpc6128,
};

// CRTC variants found in classic CPC production runs.
enum class crtc_type : u8 {
    type_0,
    type_1,
    type_2,
};

// Return the command-line name of a model.
const char *model_name(model value);

// Parse cpc464, cpc664 or cpc6128.
std::optional<model> model_from_name(const std::string &name);

// Return the conventional CPC type number of a CRTC.
int crtc_type_number(crtc_type value);

// Parse a CRTC type number from 0 through 2.
std::optional<crtc_type> crtc_type_from_number(int value);

// Configuration fixed when a machine is constructed.
struct machine_config {
    model machine_model = model::cpc6128;
    crtc_type fitted_crtc = crtc_type::type_0;
};

// A debugger view of every programmer-visible Z80 register.
struct cpu_registers {
    u16 af = 0;
    u16 bc = 0;
    u16 de = 0;
    u16 hl = 0;
    u16 af_alt = 0;
    u16 bc_alt = 0;
    u16 de_alt = 0;
    u16 hl_alt = 0;
    u16 ix = 0;
    u16 iy = 0;
    u16 sp = 0;
    u16 pc = 0;
    u16 wz = 0;
    u8 i = 0;
    u8 r = 0;
    u8 im = 0;
    bool iff1 = false;
    bool iff2 = false;
    bool halted = false;
};

// Why a bounded execution request ended.
enum class stop_reason : u8 {
    completed,
    breakpoint,
    halted,
    address_reached,
    step_limit,
};

// Return the protocol name of an execution stop reason.
const char *stop_reason_name(stop_reason reason);

// Optional limits applied to one execution request.
struct run_limits {
    u64 max_tstates = 0;
    u64 max_instructions = 0;
    u64 max_frames = 0;
    std::optional<u16> until_pc;
    bool stop_on_halt = false;
};

// Work completed by one execution request.
struct run_result {
    stop_reason reason = stop_reason::completed;
    u64 tstates = 0;
    u64 instructions = 0;
    u64 frames = 0;
    int breakpoint_id = -1;
    u16 pc = 0;
};

// One colour in the CPC hardware palette.
struct rgb {
    u8 r;
    u8 g;
    u8 b;
};

// Description of one CPC I/O decoder.
struct port_description {
    std::string name;
    std::string decode;
    bool readable = false;
    bool writable = false;
};

// The complete emulated computer.
class machine {
public:
    static constexpr int cpu_hz = 4000000;
    static constexpr int standard_tstates_per_frame = 79872;
    static constexpr int screen_width = 768;
    static constexpr int screen_height = 272;
    static constexpr int display_x = 64;
    static constexpr int display_y = 36;
    static constexpr int display_width = 640;
    static constexpr int display_height = 200;

    using frame_observer = std::function<void(u64 frame_number)>;
    using audio_observer = std::function<void(std::int16_t sample)>;

    // Construct and reset one model with a built-in development stub ROM.
    explicit machine(const machine_config &config = {});

    // Release every chip, image and observer owned by the machine.
    ~machine();

    // A machine owns pin-level component state and cannot be copied.
    machine(const machine &) = delete;

    // A machine owns pin-level component state and cannot be assigned.
    machine &operator=(const machine &) = delete;

    // Reset the hardware; optionally clear physical RAM as well.
    void reset(bool clear_memory = false);

    // Run until the first requested limit or breakpoint is reached.
    run_result run(const run_limits &limits);

    // Run for a bounded number of 4 MHz T-states.
    run_result run_tstates(u64 count);

    // Run for a number of standard 50 Hz video frames.
    run_result run_frames(u64 count);

    // Execute a number of complete Z80 instructions.
    run_result step(u64 instructions);

    // Run until an instruction begins at address or the budget expires.
    run_result run_until(u16 address, u64 max_tstates);

    // Return the selected physical CPC model.
    model machine_model() const;

    // Return the fitted CRTC variant.
    crtc_type fitted_crtc() const;

    // Return the amount of physical RAM in bytes.
    std::size_t ram_size() const;

    // Return whether this model contains a floppy controller and drive.
    bool has_disk_drive() const;

    // Read the byte visible to the CPU at an address.
    u8 read_memory(u16 address) const;

    // Write through the current memory map, including RAM behind ROM.
    void write_memory(u16 address, u8 value);

    // Read a consecutive, wrapping range of CPU-visible memory.
    std::vector<u8> read_memory(u16 address, std::size_t count) const;

    // Write a consecutive, wrapping range through the current map.
    void write_memory(u16 address, std::span<const u8> data);

    // Install the model's 16 KiB lower, BASIC and optional AMSDOS ROMs.
    bool load_roms(std::span<const u8> os, std::span<const u8> basic,
                   std::span<const u8> amsdos, std::string &error);

    // Load an MV-SNA snapshot or an AMSDOS-headered binary.
    bool quickload(std::span<const u8> data, bool start,
                   std::string &error);

    // Return whether external firmware has replaced the development stub.
    bool rom_loaded() const;

    // Return a debugger-oriented snapshot of the Z80 registers.
    cpu_registers registers() const;

    // Replace the Z80 registers and resume cleanly at regs.pc.
    void set_registers(const cpu_registers &regs);

    // Continue at an address, discarding a partially decoded instruction.
    void jump(u16 address);

    // Return whether the Z80 is currently halted.
    bool halted() const;

    // Return the address of the instruction currently being executed.
    u16 instruction_address() const;

    // Perform a diagnostic port read without advancing emulated time.
    u8 read_port(u16 port);

    // Perform a diagnostic port write without advancing emulated time.
    void write_port(u16 port, u8 value);

    // List every built-in partial I/O decoder.
    std::vector<port_description> port_map() const;

    // Press a CPC key by canonical name; return false for an unknown name.
    bool key_down(const std::string &name);

    // Release a CPC key by canonical name; return false for an unknown name.
    bool key_up(const std::string &name);

    // Release all keyboard and joystick inputs.
    void release_keys();

    // Copy the current 768 by 272 palette-indexed raster without padding.
    std::vector<u8> screen_pixels() const;

    // Copy the standard 640 by 200 active display area.
    std::vector<u8> display_pixels() const;

    // Return the 64-entry renderer palette, including sync/debug colours.
    std::vector<rgb> palette() const;

    // Return the current five-bit Gate Array border colour.
    u8 border_colour() const;

    // Return the current Gate Array video mode, including mode 3.
    u8 video_mode() const;

    // Return the selected AY register.
    u8 ay_selected_register() const;

    // Read one AY-3-8912 register directly for diagnostics.
    u8 ay_register(u8 index) const;

    // Write one AY-3-8912 register directly for diagnostics.
    void set_ay_register(u8 index, u8 value);

    // Return the current PPI control word.
    u8 ppi_control() const;

    // Return the cassette transport connected to PPI port B bit 7.
    tape_deck &tape();
    const tape_deck &tape() const;

    // Insert a standard or extended CPCEMU DSK image.
    bool insert_disk(std::span<const u8> data, std::string &error);

    // Eject the disk in drive A.
    void eject_disk();

    // Return whether drive A currently contains an image.
    bool disk_inserted() const;

    // Return whether the drive motor is currently on.
    bool disk_motor_on() const;

    // Return the breakpoint collection used by execution and bus accesses.
    breakpoint_set &breakpoints();
    const breakpoint_set &breakpoints() const;

    // Return elapsed 4 MHz T-states since reset.
    u64 total_tstates() const;

    // Return T-states within the current standard video frame.
    u32 frame_tstate() const;

    // Return completed standard video frames since reset.
    u64 frame_number() const;

    // Return retired instructions since reset.
    u64 instruction_count() const;

    // Observe each completed frame; an empty function removes the observer.
    void set_frame_observer(frame_observer observer);

    // Sample mixed AY audio at the requested host rate.
    void set_audio_observer(int sample_rate, audio_observer observer);

    // Stop producing host audio samples.
    void clear_audio_observer();

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Read a complete host file into memory.
std::optional<std::vector<u8>> read_file(const std::string &path,
                                         std::string &error);

} // namespace cpc

#endif // CPC_MACHINE_H
