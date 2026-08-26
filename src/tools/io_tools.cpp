//
// CPC keyboard, partially decoded I/O bus and chip register tools.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <memory>

#include "tools/registration.h"
#include "tools/support.h"

namespace tools {
namespace {

class press_keys_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "press_keys"; }
    std::string description() const override
    {
        return "Type text or hold one CPC keyboard chord. The machine runs "
               "while keys are held so firmware keyboard scans see them.";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .string("text", "Text to type; newline means ENTER.")
            .string_array("keys", "Named chord, such as ['CTRL','ENTER'].")
            .integer("hold_frames", "Frames per key/chord; default 2.", 1,
                     100)
            .integer("gap_frames", "Released frames between keys; default 1.",
                     0, 100)
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
        const bool has_text = arguments["text"].is_string();
        const bool has_keys = arguments["keys"].is_array();
        if (has_text == has_keys)
            return mcp::tool_result::failure(
                "supply exactly one of 'text' or 'keys'");
        const auto hold = read_int_in(arguments["hold_frames"], 1, 100, 2);
        const auto gap = read_int_in(arguments["gap_frames"], 0, 100, 1);
        if (!hold || !gap)
            return mcp::tool_result::failure("invalid hold or gap frames");

        const cpc::u64 before = machine().total_tstates();
        std::size_t presses = 0;
        if (has_text) {
            const std::string value = arguments["text"].as_string();
            if (value.size() > 256)
                return mcp::tool_result::failure("text is limited to 256 bytes");
            for (char character : value) {
                const std::string key = character == '\n'
                                            ? "ENTER"
                                            : std::string(1, character);
                if (!machine().key_down(key)) {
                    machine().release_keys();
                    return mcp::tool_result::failure(
                        "character is not present on the CPC keyboard");
                }
                machine().run_frames(*hold);
                machine().release_keys();
                if (*gap != 0)
                    machine().run_frames(*gap);
                ++presses;
            }
        } else {
            for (const json::value &item : arguments["keys"].elements()) {
                if (!item.is_string() ||
                    !machine().key_down(item.as_string())) {
                    machine().release_keys();
                    return mcp::tool_result::failure("unknown CPC key name");
                }
                ++presses;
            }
            if (presses == 0)
                return mcp::tool_result::failure("keys is empty");
            machine().run_frames(*hold);
            machine().release_keys();
        }
        json::value state = machine_state();
        state.set("presses", json::value(static_cast<std::int64_t>(presses)));
        state.set("elapsed_tstates",
                  json::value(machine().total_tstates() - before));
        return mcp::tool_result::of("sent " + std::to_string(presses) +
                                        " CPC key press(es)",
                                    std::move(state));
    }
};

class read_port_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "read_port"; }
    std::string description() const override
    {
        return "Perform a diagnostic CPC I/O read without advancing time. "
               "All matching partial decoders see the access.";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .integer("port", "16-bit I/O address.", 0, 0xffff)
            .required({"port"})
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
        const auto port = read_int_in(arguments["port"], 0, 0xffff);
        if (!port)
            return mcp::tool_result::failure("port must be 0..65535");
        const cpc::u8 value = machine().read_port(*port);
        json::value result = json::value::make_object();
        result.set("port", json::value(*port));
        result.set("value", json::value(value));
        return mcp::tool_result::of(hex16(*port) + " -> " + hex8(value),
                                    std::move(result));
    }
};

class write_port_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "write_port"; }
    std::string description() const override
    {
        return "Perform a diagnostic CPC I/O write without advancing time. "
               "Overlapping Gate Array, CRTC, PPI, printer, ROM-select and "
               "floppy decoders are all honoured.";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .integer("port", "16-bit I/O address.", 0, 0xffff)
            .integer("value", "Byte to place on the data bus.", 0, 0xff)
            .required({"port", "value"})
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
        const auto port = read_int_in(arguments["port"], 0, 0xffff);
        const auto value = read_int_in(arguments["value"], 0, 0xff);
        if (!port || !value)
            return mcp::tool_result::failure("invalid port or byte value");
        machine().write_port(*port, *value);
        json::value result = json::value::make_object();
        result.set("port", json::value(*port));
        result.set("value", json::value(*value));
        return mcp::tool_result::of(hex16(*port) + " <- " + hex8(*value),
                                    std::move(result));
    }
};

class ports_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "ports"; }
    std::string description() const override
    {
        return "List every internal CPC port decoder and its address-line "
               "condition. CPC hardware uses partial, overlapping decode.";
    }
    json::value input_schema() const override
    {
        return schema_builder().build();
    }
    mcp::tool_result invoke(const json::value &) override
    {
        json::value list = json::value::make_array();
        std::string text;
        for (const cpc::port_description &port : machine().port_map()) {
            json::value item = json::value::make_object();
            item.set("name", json::value(port.name));
            item.set("decode", json::value(port.decode));
            item.set("readable", json::value(port.readable));
            item.set("writable", json::value(port.writable));
            list.push_back(std::move(item));
            if (!text.empty())
                text += '\n';
            text += port.name + ": " + port.decode;
        }
        return mcp::tool_result::of(std::move(text), std::move(list));
    }
};

class chips_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "chips"; }
    std::string description() const override
    {
        return "Inspect Gate Array, PPI and all sixteen AY-3-8912 registers; "
               "optionally write one AY register directly.";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .integer("ay_register", "AY register to write.", 0, 15)
            .integer("ay_value", "AY value to write.", 0, 255)
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
        if (!arguments["ay_register"].is_null() ||
            !arguments["ay_value"].is_null()) {
            const auto reg = read_int_in(arguments["ay_register"], 0, 15);
            const auto value = read_int_in(arguments["ay_value"], 0, 255);
            if (!reg || !value)
                return mcp::tool_result::failure(
                    "ay_register and ay_value must be supplied together");
            machine().set_ay_register(*reg, *value);
        }
        json::value state = json::value::make_object();
        state.set("video_mode", json::value(machine().video_mode()));
        state.set("border", json::value(machine().border_colour()));
        state.set("ppi_control", json::value(machine().ppi_control()));
        state.set("ay_selected", json::value(
                      machine().ay_selected_register()));
        json::value ay = json::value::make_array();
        for (int i = 0; i < 16; ++i)
            ay.push_back(json::value(machine().ay_register(i)));
        state.set("ay", std::move(ay));
        return mcp::tool_result::of("CPC chip register state",
                                    std::move(state));
    }
};

} // namespace

void register_io_tools(mcp::tool_registry &registry, cpc::machine &target)
{
    registry.add(std::make_unique<press_keys_tool>(target));
    registry.add(std::make_unique<read_port_tool>(target));
    registry.add(std::make_unique<write_port_tool>(target));
    registry.add(std::make_unique<ports_tool>(target));
    registry.add(std::make_unique<chips_tool>(target));
}

} // namespace tools
