//
// Implementation of the classic Amstrad CPC facade and master clock.
//
// The third-party chips core performs one Z80 and Gate Array T-state per
// call. This wrapper adds deterministic run limits, breakpoint observation,
// model selection, host media, frame capture and debugger-safe direct access.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "cpc/machine.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_set>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define CHIPS_IMPL
#include "chips/chips_common.h"
#include "z80/z80.h"
#include "chips/ay38910.h"
#include "chips/i8255.h"
#include "chips/mc6845.h"
#include "chips/am40010.h"
#include "chips/upd765.h"
#include "chips/mem.h"
#include "chips/kbd.h"
#include "chips/clk.h"
#include "chips/fdd.h"
#include "chips/fdd_cpc.h"
#include "chips/cpc_system.h"
#pragma GCC diagnostic pop

namespace cpc {

namespace {

constexpr u64 default_tstate_budget =
    static_cast<u64>(machine::standard_tstates_per_frame);
constexpr int internal_audio_rate = 44100;

constexpr int key_shift = 0x80;
constexpr int key_control = 0x81;
constexpr int key_tab = 0x82;
constexpr int key_caps_lock = 0x83;
constexpr int key_copy = 0x84;

struct model_entry {
    model value;
    const char *name;
};

constexpr model_entry models[] = {
    {model::cpc464, "cpc464"},
    {model::cpc664, "cpc664"},
    {model::cpc6128, "cpc6128"},
};

cpc_type_t chips_model(model value)
{
    switch (value) {
    case model::cpc464:
        return CPC_TYPE_464;
    case model::cpc664:
        return CPC_TYPE_664;
    case model::cpc6128:
        return CPC_TYPE_6128;
    }
    return CPC_TYPE_6128;
}

mc6845_type_t chips_crtc(crtc_type value)
{
    switch (value) {
    case crtc_type::type_0:
        return MC6845_TYPE_UM6845;
    case crtc_type::type_1:
        return MC6845_TYPE_UM6845R;
    case crtc_type::type_2:
        return MC6845_TYPE_MC6845;
    }
    return MC6845_TYPE_UM6845;
}

std::string uppercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::toupper(c));
                   });
    return value;
}

std::optional<int> named_key(const std::string &name)
{
    if (name.size() == 1)
        return static_cast<unsigned char>(name.front());

    const std::string key = uppercase(name);
    if (key == "SPACE")
        return 0x20;
    if (key == "LEFT")
        return 0x08;
    if (key == "RIGHT")
        return 0x09;
    if (key == "DOWN")
        return 0x0a;
    if (key == "UP")
        return 0x0b;
    if (key == "DELETE" || key == "DEL" || key == "BACKSPACE")
        return 0x01;
    if (key == "CLEAR" || key == "CLR")
        return 0x0c;
    if (key == "ENTER" || key == "RETURN")
        return 0x0d;
    if (key == "ESC" || key == "ESCAPE")
        return 0x03;
    if (key == "SHIFT")
        return key_shift;
    if (key == "CTRL" || key == "CONTROL")
        return key_control;
    if (key == "TAB")
        return key_tab;
    if (key == "CAPS" || key == "CAPS_LOCK")
        return key_caps_lock;
    if (key == "COPY")
        return key_copy;
    if (key.size() == 2 && key[0] == 'F' && key[1] >= '0' &&
        key[1] <= '9') {
        return key[1] == '0' ? 0xfa : 0xf0 + (key[1] - '0');
    }
    return std::nullopt;
}

void register_extra_keys(cpc_t &system)
{
    kbd_register_key(&system.kbd, key_shift, 2, 5, 0);
    kbd_register_key(&system.kbd, key_control, 2, 7, 0);
    kbd_register_key(&system.kbd, key_tab, 8, 4, 0);
    kbd_register_key(&system.kbd, key_caps_lock, 8, 6, 0);
    kbd_register_key(&system.kbd, key_copy, 1, 1, 0);
}

void set_standard_crtc(mc6845_t &crtc)
{
    constexpr std::array<u8, 18> registers = {
        63, 40, 46, 0x8e, 38, 0, 25, 30, 0, 7,
        0, 0, 0x30, 0, 0, 0, 0, 0,
    };
    std::copy(registers.begin(), registers.end(), crtc.reg);
}

} // namespace

const char *model_name(model value)
{
    for (const model_entry &entry : models) {
        if (entry.value == value)
            return entry.name;
    }
    return "cpc6128";
}

std::optional<model> model_from_name(const std::string &name)
{
    const std::string wanted = uppercase(name);
    for (const model_entry &entry : models) {
        if (uppercase(entry.name) == wanted)
            return entry.value;
    }
    if (wanted == "464")
        return model::cpc464;
    if (wanted == "664")
        return model::cpc664;
    if (wanted == "6128")
        return model::cpc6128;
    return std::nullopt;
}

int crtc_type_number(crtc_type value)
{
    return static_cast<int>(value);
}

std::optional<crtc_type> crtc_type_from_number(int value)
{
    if (value < 0 || value > 2)
        return std::nullopt;
    return static_cast<crtc_type>(value);
}

const char *stop_reason_name(stop_reason reason)
{
    switch (reason) {
    case stop_reason::completed:
        return "completed";
    case stop_reason::breakpoint:
        return "breakpoint";
    case stop_reason::halted:
        return "halted";
    case stop_reason::address_reached:
        return "address_reached";
    case stop_reason::step_limit:
        return "step_limit";
    }
    return "completed";
}

struct machine::impl {
    enum class prefix_kind : u8 {
        none,
        indexed,
        cb,
        ed,
        indexed_cb,
    };

    struct tick_result {
        bool retired = false;
        int breakpoint_id = -1;
    };

    void begin_instruction(u8 opcode)
    {
        instruction_active = true;
        flags_before_instruction = system.cpu.f;
        prefix = prefix_kind::none;
        observe_opcode(opcode);
    }

    void observe_opcode(u8 opcode)
    {
        if (prefix == prefix_kind::cb || prefix == prefix_kind::ed ||
            prefix == prefix_kind::indexed_cb) {
            return;
        }

        if (opcode == 0xdd || opcode == 0xfd) {
            prefix = prefix_kind::indexed;
        } else if (opcode == 0xed) {
            prefix = prefix_kind::ed;
        } else if (opcode == 0xcb) {
            prefix = prefix == prefix_kind::indexed
                         ? prefix_kind::indexed_cb
                         : prefix_kind::cb;
        }
    }

    static bool base_opcode_changes_flags(u8 opcode)
    {
        if (opcode >= 0x80 && opcode <= 0xbf)
            return true;
        if ((opcode & 0xc7) == 0x04 || (opcode & 0xc7) == 0x05)
            return true;
        if ((opcode & 0xcf) == 0x09)
            return true;

        switch (opcode) {
        case 0x07:
        case 0x0f:
        case 0x17:
        case 0x1f:
        case 0x27:
        case 0x2f:
        case 0x37:
        case 0x3f:
        case 0xc6:
        case 0xce:
        case 0xd6:
        case 0xde:
        case 0xe6:
        case 0xee:
        case 0xf6:
        case 0xfe:
            return true;
        default:
            return false;
        }
    }

    static bool ed_opcode_changes_flags(u8 opcode)
    {
        if ((opcode & 0xc7) == 0x40)
            return true;
        if ((opcode & 0xcf) == 0x42 || (opcode & 0xcf) == 0x4a)
            return true;
        if ((opcode & 0xc7) == 0x44)
            return true;

        switch (opcode) {
        case 0x57:
        case 0x5f:
        case 0x67:
        case 0x6f:
        case 0xa0:
        case 0xa1:
        case 0xa2:
        case 0xa3:
        case 0xa8:
        case 0xa9:
        case 0xaa:
        case 0xab:
        case 0xb0:
        case 0xb1:
        case 0xb2:
        case 0xb3:
        case 0xb8:
        case 0xb9:
        case 0xba:
        case 0xbb:
            return true;
        default:
            return false;
        }
    }

    bool current_instruction_changes_flags() const
    {
        if (prefix == prefix_kind::cb ||
            prefix == prefix_kind::indexed_cb) {
            return (system.cpu.opcode >> 6) < 2;
        }
        if (prefix == prefix_kind::ed)
            return ed_opcode_changes_flags(system.cpu.opcode);
        return base_opcode_changes_flags(system.cpu.opcode);
    }

    void finish_instruction()
    {
        if (!instruction_active)
            return;

        z80_t &cpu = system.cpu;
        const bool base = prefix == prefix_kind::none ||
                          prefix == prefix_kind::indexed;
        if (base && (cpu.opcode == 0x37 || cpu.opcode == 0x3f)) {
            constexpr u8 undocumented = 0x28;
            const u8 sources = last_instruction_changed_flags
                                   ? cpu.a
                                   : static_cast<u8>(
                                         flags_before_instruction | cpu.a);
            cpu.f = static_cast<u8>((cpu.f & ~undocumented) |
                                     (sources & undocumented));
        }

        if (prefix == prefix_kind::ed) {
            switch (cpu.opcode) {
            case 0xb2: cpu.wz = static_cast<u16>(cpu.bc + 0x0101); break;
            case 0xba: cpu.wz = static_cast<u16>(cpu.bc + 0x00ff); break;
            case 0xb3: cpu.wz = static_cast<u16>(cpu.bc + 1); break;
            case 0xbb: cpu.wz = static_cast<u16>(cpu.bc - 1); break;
            default: break;
            }
        }

        last_instruction_changed_flags =
            current_instruction_changes_flags();
        instruction_active = false;
    }

    void reset_instruction_state()
    {
        last_instruction_changed_flags = false;
        instruction_active = false;
        flags_before_instruction = 0;
        prefix = prefix_kind::none;
    }

    void accept_interrupt(u8 opcode)
    {
        finish_instruction();
        reset_instruction_state();
        if (system.cpu.im == 0)
            begin_instruction(opcode);
    }

    explicit impl(const machine_config &requested) : config(requested)
    {
        std::array<u8, 0x4000> os{};
        std::array<u8, 0x4000> basic{};
        std::array<u8, 0x4000> amsdos{};
        os.fill(0x76);
        basic.fill(0xff);
        amsdos.fill(0xff);

        cpc_desc_t description{};
        description.type = chips_model(config.machine_model);
        description.crtc_type = chips_crtc(config.fitted_crtc);
        description.joystick_type = CPC_JOYSTICK_NONE;
        description.audio.callback.func = audio_callback;
        description.audio.callback.user_data = this;
        description.audio.num_samples = 1;
        description.audio.sample_rate = internal_audio_rate;
        description.audio.volume = 0.5f;

        description.roms.cpc464.os = {os.data(), os.size()};
        description.roms.cpc464.basic = {basic.data(), basic.size()};
        description.roms.cpc664.os = {os.data(), os.size()};
        description.roms.cpc664.basic = {basic.data(), basic.size()};
        description.roms.cpc664.amsdos = {amsdos.data(), amsdos.size()};
        description.roms.cpc6128.os = {os.data(), os.size()};
        description.roms.cpc6128.basic = {basic.data(), basic.size()};
        description.roms.cpc6128.amsdos = {amsdos.data(), amsdos.size()};

        cpc_init(&system, &description);
        register_extra_keys(system);
    }

    ~impl()
    {
        if (system.valid)
            cpc_discard(&system);
    }

    static void audio_callback(const float *samples, int count,
                               void *user_data)
    {
        auto &self = *static_cast<impl *>(user_data);
        if (!self.audio_sink)
            return;
        for (int i = 0; i < count; ++i) {
            const float limited = std::clamp(samples[i], -1.0f, 1.0f);
            self.audio_sink(static_cast<std::int16_t>(limited * 32767.0f));
        }
    }

    void reset_state(bool clear_memory)
    {
        if (clear_memory)
            std::memset(system.ram, 0, sizeof system.ram);

        cpc_reset(&system);
        _am40010_init_crt(&system.ga);
        // The first Z80 T1 follows the Gate Array's ready slot. Starting the
        // divided sequencer one count before that slot avoids a spurious
        // three-state reset wait and matches the power-on clock phase.
        system.ga.seq_tick_count = 3;
        fdd_motor(&system.fdd, false);
        deck.reset();

        if (!external_rom) {
            set_standard_crtc(system.crtc);
            system.ga.regs.config = 1;
            system.ga.video.mode = 1;
            system.ga.regs.border = 20;
            system.ga.regs.ink[0] = 20;
            system.ga.regs.ink[1] = 11;
        }

        total_tstates = 0;
        frame_tstate = 0;
        frame_number = 0;
        instructions = 0;
        instruction_address = 0;
        bus_active = false;
        boundary_seen = false;
        reset_instruction_state();
        pending_breakpoint = -1;
        pending_retirement = false;
        pending_retirement_breakpoint = -1;
        retirement_phase = -1;
        pressed_keys.clear();
    }

    tick_result tick()
    {
        system.tape_input = deck.ear_level();
        system.pins = _cpc_tick(&system, system.pins);

        tick_result result;
        const bool memory_request = (system.pins & Z80_MREQ) != 0;
        const bool io_request = (system.pins & Z80_IORQ) != 0;
        const bool active = memory_request || io_request;

        if (active && !bus_active) {
            const auto address = static_cast<u16>(Z80_GET_ADDR(system.pins));
            const auto value = static_cast<u8>(Z80_GET_DATA(system.pins));
            const bool first_cycle = (system.pins & Z80_M1) != 0;
            const bool refresh = (system.pins & Z80_RFSH) != 0;

            if (memory_request && !refresh) {
                if (first_cycle && z80_opdone(&system.cpu)) {
                    finish_instruction();
                    begin_instruction(value);
                    instruction_address = address;
                    if (boundary_seen) {
                        int retirement_breakpoint = -1;
                        if (pending_breakpoint >= 0) {
                            retirement_breakpoint = pending_breakpoint;
                            pending_breakpoint = -1;
                        } else if (breakpoint *hit = breakpoints.match(
                                       breakpoint_kind::execute,
                                       instruction_address, 0)) {
                            retirement_breakpoint = hit->id;
                        }
                        pending_retirement = true;
                        pending_retirement_breakpoint =
                            retirement_breakpoint;
                    } else {
                        boundary_seen = true;
                        retirement_phase =
                            static_cast<int>(total_tstates & 3);
                    }
                } else if (first_cycle && (system.pins & Z80_RD)) {
                    observe_opcode(value);
                } else if (!first_cycle && (system.pins & Z80_RD)) {
                    if (breakpoint *hit = breakpoints.match(
                            breakpoint_kind::memory_read, address, value))
                        pending_breakpoint = hit->id;
                } else if (system.pins & Z80_WR) {
                    if (breakpoint *hit = breakpoints.match(
                            breakpoint_kind::memory_write, address, value))
                        pending_breakpoint = hit->id;
                }
            } else if (io_request) {
                if (first_cycle) {
                    accept_interrupt(value);
                } else if (system.pins & Z80_RD) {
                    if (breakpoint *hit = breakpoints.match(
                            breakpoint_kind::io_read, address, value))
                        pending_breakpoint = hit->id;
                } else if (system.pins & Z80_WR) {
                    if (breakpoint *hit = breakpoints.match(
                            breakpoint_kind::io_write, address, value))
                        pending_breakpoint = hit->id;
                }
            }
        }
        bus_active = active;

        // The core exposes the overlapped next opcode fetch as soon as its
        // T1 begins. On a CPC, a preceding three-T-state M-cycle is still
        // being rounded to the next Gate Array slot at that point. Retire the
        // instruction only on the globally aligned four-T-state boundary.
        if (pending_retirement && retirement_phase >= 0 &&
            static_cast<int>(total_tstates & 3) == retirement_phase) {
            ++instructions;
            result.retired = true;
            result.breakpoint_id = pending_retirement_breakpoint;
            pending_retirement = false;
            pending_retirement_breakpoint = -1;
        }

        ++total_tstates;
        ++frame_tstate;
        if (system.tape_motor_on)
            deck.tick();

        if (frame_tstate >= machine::standard_tstates_per_frame) {
            frame_tstate = 0;
            ++frame_number;
            kbd_update(&system.kbd, 19968);
            if (frame_sink)
                frame_sink(frame_number);
        }
        return result;
    }

    u8 direct_io(u16 port, u8 value, bool write)
    {
        u64 cpu_pins = static_cast<u64>(port) | Z80_IORQ |
                       (write ? Z80_WR : Z80_RD);
        Z80_SET_DATA(cpu_pins, value);

        if ((cpu_pins & Z80_A11) == 0) {
            u64 ppi_pins = (cpu_pins & Z80_PIN_MASK &
                ~(I8255_PC_PINS | I8255_A1 | I8255_A0)) | I8255_CS;
            if (cpu_pins & Z80_A9)
                ppi_pins |= I8255_A1;
            if (cpu_pins & Z80_A8)
                ppi_pins |= I8255_A0;

            if ((system.ppi.pins & (I8255_PC7 | I8255_PC6)) != 0) {
                u64 ay_pins = 0;
                if (system.ppi.pins & I8255_PC7)
                    ay_pins |= AY38910_BDIR;
                if (system.ppi.pins & I8255_PC6)
                    ay_pins |= AY38910_BC1;
                AY38910_SET_DATA(ay_pins, I8255_GET_PA(system.ppi.pins));
                ay_pins = ay38910_iorq(&system.psg, ay_pins);
                I8255_SET_PA(ppi_pins, AY38910_GET_DATA(ay_pins));
            }

            ppi_pins |= I8255_PB1 | I8255_PB2 | I8255_PB3 | I8255_PB4;
            if (system.tape_input)
                ppi_pins |= I8255_PB7;
            if (system.crtc.vs)
                ppi_pins |= I8255_PB0;
            ppi_pins = i8255_tick(&system.ppi, ppi_pins);

            if ((ppi_pins & (I8255_CS | I8255_RD)) ==
                (I8255_CS | I8255_RD)) {
                Z80_SET_DATA(cpu_pins, I8255_GET_DATA(ppi_pins));
            }

            if ((ppi_pins & (I8255_PC7 | I8255_PC6)) != 0) {
                u64 ay_pins = 0;
                if (ppi_pins & I8255_PC7)
                    ay_pins |= AY38910_BDIR;
                if (ppi_pins & I8255_PC6)
                    ay_pins |= AY38910_BC1;
                AY38910_SET_DATA(ay_pins, I8255_GET_PA(ppi_pins));
                ay38910_iorq(&system.psg, ay_pins);
            }

            kbd_set_active_columns(
                &system.kbd, static_cast<u16>(1u <<
                    (I8255_GET_PC(ppi_pins) & 0x0f)));
            system.tape_motor_on =
                0 != (I8255_GET_PC(ppi_pins) & (1 << 4));
            system.tape_output =
                0 != (I8255_GET_PC(ppi_pins) & (1 << 5));
        }

        am40010_iorq(&system.ga, cpu_pins);

        if (((cpu_pins & Z80_A12) == 0) && write)
            system.printer_data = Z80_GET_DATA(cpu_pins);

        if ((cpu_pins & Z80_A14) == 0) {
            u64 crtc_pins = (cpu_pins & Z80_PIN_MASK) | MC6845_CS;
            if (cpu_pins & Z80_A9)
                crtc_pins |= MC6845_RW;
            if (cpu_pins & Z80_A8)
                crtc_pins |= MC6845_RS;
            cpu_pins = mc6845_iorq(&system.crtc, crtc_pins) & Z80_PIN_MASK;
        }

        if (config.machine_model != model::cpc464) {
            const u64 decode = cpu_pins & (Z80_A10 | Z80_A8 | Z80_A7);
            if (decode == 0 && write) {
                fdd_motor(&system.fdd,
                          0 != (Z80_GET_DATA(cpu_pins) & 1));
            } else if (decode == Z80_A8) {
                u64 fdc_pins = UPD765_CS | (cpu_pins & Z80_PIN_MASK);
                cpu_pins = upd765_iorq(&system.fdc, fdc_pins) &
                           Z80_PIN_MASK;
            }
        }
        return static_cast<u8>(Z80_GET_DATA(cpu_pins));
    }

    machine_config config;
    cpc_t system{};
    tape_deck deck;
    breakpoint_set breakpoints;
    frame_observer frame_sink;
    audio_observer audio_sink;
    std::unordered_set<int> pressed_keys;
    u64 total_tstates = 0;
    u32 frame_tstate = 0;
    u64 frame_number = 0;
    u64 instructions = 0;
    u16 instruction_address = 0;
    int pending_breakpoint = -1;
    bool bus_active = false;
    bool boundary_seen = false;
    bool external_rom = false;
    bool pending_retirement = false;
    int pending_retirement_breakpoint = -1;
    int retirement_phase = -1;
    bool last_instruction_changed_flags = false;
    bool instruction_active = false;
    u8 flags_before_instruction = 0;
    prefix_kind prefix = prefix_kind::none;
};

machine::machine(const machine_config &config)
    : impl_(std::make_unique<impl>(config))
{
    impl_->reset_state(true);
}

machine::~machine() = default;

void machine::reset(bool clear_memory)
{
    impl_->reset_state(clear_memory);
}

run_result machine::run(const run_limits &limits)
{
    const u64 start_tstates = impl_->total_tstates;
    const u64 start_instructions = impl_->instructions;
    const u64 start_frames = impl_->frame_number;
    u64 budget = limits.max_tstates;
    if (budget == 0 && limits.max_instructions == 0 &&
        limits.max_frames == 0 && !limits.until_pc &&
        !limits.stop_on_halt) {
        budget = default_tstate_budget;
    }

    run_result result;
    if (limits.until_pc && instruction_address() == *limits.until_pc) {
        result.reason = stop_reason::address_reached;
        result.pc = instruction_address();
        return result;
    }

    while (true) {
        const impl::tick_result tick = impl_->tick();

        if (tick.retired) {
            if (tick.breakpoint_id >= 0) {
                result.reason = stop_reason::breakpoint;
                result.breakpoint_id = tick.breakpoint_id;
                break;
            }
            if (limits.until_pc &&
                instruction_address() == *limits.until_pc) {
                result.reason = stop_reason::address_reached;
                break;
            }
            if (limits.stop_on_halt && halted()) {
                result.reason = stop_reason::halted;
                break;
            }
            if (limits.max_instructions != 0 &&
                impl_->instructions - start_instructions >=
                    limits.max_instructions) {
                result.reason = stop_reason::step_limit;
                break;
            }
        }

        if (limits.max_frames != 0 &&
            impl_->frame_number - start_frames >= limits.max_frames)
            break;
        if (budget != 0 && impl_->total_tstates - start_tstates >= budget)
            break;
    }

    result.tstates = impl_->total_tstates - start_tstates;
    result.instructions = impl_->instructions - start_instructions;
    result.frames = impl_->frame_number - start_frames;
    result.pc = instruction_address();
    return result;
}

run_result machine::run_tstates(u64 count)
{
    run_limits limits;
    limits.max_tstates = count;
    return run(limits);
}

run_result machine::run_frames(u64 count)
{
    run_limits limits;
    limits.max_frames = count;
    return run(limits);
}

run_result machine::step(u64 instructions)
{
    run_limits limits;
    limits.max_instructions = instructions;
    return run(limits);
}

run_result machine::run_until(u16 address, u64 max_tstates)
{
    run_limits limits;
    limits.until_pc = address;
    limits.max_tstates = max_tstates;
    return run(limits);
}

model machine::machine_model() const { return impl_->config.machine_model; }

crtc_type machine::fitted_crtc() const { return impl_->config.fitted_crtc; }

std::size_t machine::ram_size() const
{
    return machine_model() == model::cpc6128 ? 128u * 1024u : 64u * 1024u;
}

bool machine::has_disk_drive() const
{
    return machine_model() != model::cpc464;
}

u8 machine::read_memory(u16 address) const
{
    return mem_rd(const_cast<mem_t *>(&impl_->system.mem), address);
}

void machine::write_memory(u16 address, u8 value)
{
    mem_wr(&impl_->system.mem, address, value);
}

std::vector<u8> machine::read_memory(u16 address, std::size_t count) const
{
    std::vector<u8> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        result.push_back(read_memory(static_cast<u16>(address + i)));
    return result;
}

void machine::write_memory(u16 address, std::span<const u8> data)
{
    for (std::size_t i = 0; i < data.size(); ++i)
        write_memory(static_cast<u16>(address + i), data[i]);
}

bool machine::load_roms(std::span<const u8> os,
                        std::span<const u8> basic,
                        std::span<const u8> amsdos,
                        std::string &error)
{
    constexpr std::size_t rom_size = 0x4000;
    if (os.size() != rom_size || basic.size() != rom_size) {
        error = "OS and BASIC ROM images must each be exactly 16384 bytes";
        return false;
    }
    if (has_disk_drive() && amsdos.size() != rom_size) {
        error = "CPC664 and CPC6128 need a 16384-byte AMSDOS ROM";
        return false;
    }

    std::copy(os.begin(), os.end(), impl_->system.rom_os);
    std::copy(basic.begin(), basic.end(), impl_->system.rom_basic);
    if (!amsdos.empty())
        std::copy(amsdos.begin(), amsdos.end(), impl_->system.rom_amsdos);
    impl_->external_rom = true;
    _cpc_bankswitch(impl_->system.ga.ram_config,
                    impl_->system.ga.regs.config,
                    impl_->system.ga.rom_select, &impl_->system);
    reset(true);
    error.clear();
    return true;
}

bool machine::rom_loaded() const { return impl_->external_rom; }

bool machine::quickload(std::span<const u8> data, bool start,
                        std::string &error)
{
    if (data.empty()) {
        error = "quickload image is empty";
        return false;
    }
    chips_range_t range{const_cast<u8 *>(data.data()), data.size()};
    if (!cpc_quickload(&impl_->system, range, start)) {
        error = "not a valid MV-SNA snapshot or AMSDOS binary";
        return false;
    }
    if (data.size() >= 8 &&
        std::equal(data.begin(), data.begin() + 8, "MV - SNA")) {
        impl_->system.pins = z80_prefetch(&impl_->system.cpu,
                                          impl_->system.cpu.pc);
        impl_->system.cpu.pins = 0;
        impl_->instruction_address = impl_->system.cpu.pc;
        impl_->bus_active = false;
        impl_->boundary_seen = false;
        impl_->reset_instruction_state();
        impl_->pending_retirement = false;
        impl_->pending_retirement_breakpoint = -1;
        impl_->retirement_phase = -1;
    }
    error.clear();
    return true;
}

cpu_registers machine::registers() const
{
    const z80_t &cpu = impl_->system.cpu;
    cpu_registers result;
    result.af = cpu.af;
    result.bc = cpu.bc;
    result.de = cpu.de;
    result.hl = cpu.hl;
    result.af_alt = cpu.af2;
    result.bc_alt = cpu.bc2;
    result.de_alt = cpu.de2;
    result.hl_alt = cpu.hl2;
    result.ix = cpu.ix;
    result.iy = cpu.iy;
    result.sp = cpu.sp;
    result.pc = instruction_address();
    result.wz = cpu.wz;
    result.i = cpu.i;
    result.r = cpu.r;
    result.im = cpu.im;
    result.iff1 = cpu.iff1;
    result.iff2 = cpu.iff2;
    result.halted = halted();
    return result;
}

void machine::set_registers(const cpu_registers &regs)
{
    z80_t &cpu = impl_->system.cpu;
    cpu.af = regs.af;
    cpu.bc = regs.bc;
    cpu.de = regs.de;
    cpu.hl = regs.hl;
    cpu.af2 = regs.af_alt;
    cpu.bc2 = regs.bc_alt;
    cpu.de2 = regs.de_alt;
    cpu.hl2 = regs.hl_alt;
    cpu.ix = regs.ix;
    cpu.iy = regs.iy;
    cpu.sp = regs.sp;
    cpu.wz = regs.wz;
    cpu.i = regs.i;
    cpu.r = regs.r;
    cpu.im = regs.im;
    cpu.iff1 = regs.iff1;
    cpu.iff2 = regs.iff2;
    impl_->system.pins = z80_prefetch(&cpu, regs.pc);
    cpu.pins = 0;
    impl_->instruction_address = regs.pc;
    impl_->bus_active = false;
    impl_->boundary_seen = false;
    impl_->reset_instruction_state();
    impl_->pending_retirement = false;
    impl_->pending_retirement_breakpoint = -1;
    impl_->retirement_phase = -1;
}

void machine::jump(u16 address)
{
    impl_->system.pins = z80_prefetch(&impl_->system.cpu, address);
    impl_->system.cpu.pins = 0;
    impl_->instruction_address = address;
    impl_->bus_active = false;
    impl_->boundary_seen = false;
    impl_->reset_instruction_state();
    impl_->pending_retirement = false;
    impl_->pending_retirement_breakpoint = -1;
    impl_->retirement_phase = -1;
}

bool machine::halted() const
{
    return (impl_->system.pins & Z80_HALT) != 0;
}

u16 machine::instruction_address() const
{
    return impl_->instruction_address;
}

u8 machine::read_port(u16 port)
{
    return impl_->direct_io(port, 0xff, false);
}

void machine::write_port(u16 port, u8 value)
{
    (void)impl_->direct_io(port, value, true);
}

std::vector<port_description> machine::port_map() const
{
    std::vector<port_description> result = {
        {"gate_array", "A15=0,A14=1", false, true},
        {"crtc", "A14=0; A9=R/W, A8=RS", true, true},
        {"upper_rom_select", "A13=0", false, true},
        {"printer", "A12=0", false, true},
        {"ppi_8255", "A11=0; A9,A8=register", true, true},
        {"expansion", "A10=0; no generic device attached", false, false},
    };
    if (has_disk_drive()) {
        result.push_back(
            {"fdc_motor", "A10=0,A8=0,A7=0", false, true});
        result.push_back(
            {"upd765a", "A10=0,A8=1,A7=0", true, true});
    }
    return result;
}

bool machine::key_down(const std::string &name)
{
    const std::optional<int> key = named_key(name);
    if (!key)
        return false;
    cpc_key_down(&impl_->system, *key);
    impl_->pressed_keys.insert(*key);
    return true;
}

bool machine::key_up(const std::string &name)
{
    const std::optional<int> key = named_key(name);
    if (!key)
        return false;
    cpc_key_up(&impl_->system, *key);
    impl_->pressed_keys.erase(*key);
    return true;
}

void machine::release_keys()
{
    for (int key : impl_->pressed_keys)
        cpc_key_up(&impl_->system, key);
    impl_->pressed_keys.clear();
}

std::vector<u8> machine::screen_pixels() const
{
    std::vector<u8> result;
    result.reserve(static_cast<std::size_t>(screen_width) * screen_height);
    for (int y = 0; y < screen_height; ++y) {
        const u8 *row = impl_->system.fb +
                        static_cast<std::size_t>(y) *
                            AM40010_FRAMEBUFFER_WIDTH;
        result.insert(result.end(), row, row + screen_width);
    }
    return result;
}

std::vector<u8> machine::display_pixels() const
{
    std::vector<u8> result;
    result.reserve(static_cast<std::size_t>(display_width) * display_height);
    for (int y = 0; y < display_height; ++y) {
        const u8 *row = impl_->system.fb +
            static_cast<std::size_t>(display_y + y) *
                AM40010_FRAMEBUFFER_WIDTH + display_x;
        result.insert(result.end(), row, row + display_width);
    }
    return result;
}

std::vector<rgb> machine::palette() const
{
    std::vector<rgb> result;
    result.reserve(AM40010_NUM_HWCOLORS);
    for (int i = 0; i < AM40010_NUM_HWCOLORS; ++i) {
        const u32 colour = impl_->system.ga.hw_colors[i];
        result.push_back({static_cast<u8>(colour),
                          static_cast<u8>(colour >> 8),
                          static_cast<u8>(colour >> 16)});
    }
    return result;
}

u8 machine::border_colour() const { return impl_->system.ga.regs.border; }

u8 machine::video_mode() const { return impl_->system.ga.video.mode; }

u8 machine::ay_selected_register() const { return impl_->system.psg.addr; }

u8 machine::ay_register(u8 index) const
{
    return index < AY38910_NUM_REGISTERS ? impl_->system.psg.reg[index] : 0xff;
}

void machine::set_ay_register(u8 index, u8 value)
{
    if (index < AY38910_NUM_REGISTERS)
        ay38910_set_register(&impl_->system.psg, index, value);
}

u8 machine::ppi_control() const { return impl_->system.ppi.control; }

tape_deck &machine::tape() { return impl_->deck; }

const tape_deck &machine::tape() const { return impl_->deck; }

bool machine::insert_disk(std::span<const u8> data, std::string &error)
{
    if (!has_disk_drive()) {
        error = "the CPC464 has no built-in floppy controller";
        return false;
    }
    chips_range_t range{const_cast<u8 *>(data.data()), data.size()};
    if (!data.empty() && cpc_insert_disc(&impl_->system, range)) {
        error.clear();
        return true;
    }
    error = "not a valid standard or extended CPCEMU DSK image";
    return false;
}

void machine::eject_disk() { cpc_remove_disc(&impl_->system); }

bool machine::disk_inserted() const
{
    return fdd_disc_inserted(const_cast<fdd_t *>(&impl_->system.fdd));
}

bool machine::disk_motor_on() const { return impl_->system.fdd.motor_on; }

breakpoint_set &machine::breakpoints() { return impl_->breakpoints; }

const breakpoint_set &machine::breakpoints() const
{
    return impl_->breakpoints;
}

u64 machine::total_tstates() const { return impl_->total_tstates; }

u32 machine::frame_tstate() const { return impl_->frame_tstate; }

u64 machine::frame_number() const { return impl_->frame_number; }

u64 machine::instruction_count() const { return impl_->instructions; }

void machine::set_frame_observer(frame_observer observer)
{
    impl_->frame_sink = std::move(observer);
}

void machine::set_audio_observer(int sample_rate, audio_observer observer)
{
    if (sample_rate <= 0 || sample_rate > cpu_hz) {
        clear_audio_observer();
        return;
    }
    impl_->system.psg.sample_period =
        (1000000 * AY38910_FIXEDPOINT_SCALE) / sample_rate;
    impl_->system.psg.sample_counter = impl_->system.psg.sample_period;
    impl_->audio_sink = std::move(observer);
}

void machine::clear_audio_observer() { impl_->audio_sink = {}; }

std::optional<std::vector<u8>> read_file(const std::string &path,
                                         std::string &error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open '" + path + "'";
        return std::nullopt;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0) {
        error = "cannot determine the size of '" + path + "'";
        return std::nullopt;
    }
    file.seekg(0);
    std::vector<u8> data(static_cast<std::size_t>(size));
    if (!data.empty()) {
        file.read(reinterpret_cast<char *>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }
    if (!file) {
        error = "cannot read '" + path + "'";
        return std::nullopt;
    }
    error.clear();
    return data;
}

} // namespace cpc
