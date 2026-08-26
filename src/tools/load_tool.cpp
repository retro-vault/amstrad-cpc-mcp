//
// Host-file and inline media loading for CPC development.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cctype>
#include <memory>

#include "tools/registration.h"
#include "tools/support.h"

namespace tools {
namespace {

std::string extension(const std::string &path)
{
    const std::size_t dot = path.find_last_of('.');
    std::string result = dot == std::string::npos ? "binary"
                                                  : path.substr(dot + 1);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    if (result == "bin")
        return "binary";
    if (result == "cdt")
        return "cdt";
    if (result == "rom")
        return "rom";
    return result;
}

class load_tool final : public machine_tool {
public:
    using machine_tool::machine_tool;
    std::string name() const override { return "load"; }
    std::string description() const override
    {
        return "Load raw Z80 code, CPC CDT or TAP cassette media, a "
               "CPCEMU DSK, or a concatenated firmware ROM set. Supply one "
               "of path or inline data. Binary defaults to 0x8000.";
    }
    json::value input_schema() const override
    {
        return schema_builder()
            .string("path", "Host path to load.")
            .string("data", "Inline hex bytes or a JSON byte array.")
            .string("format", "Omit to use the path extension.",
                    {"binary", "amsdos", "sna", "cdt", "tap",
                     "dsk", "rom"})
            .integer("address", "Binary load address; default 0x8000.",
                     0, 0xffff)
            .integer("start", "Set PC after a binary load.", 0, 0xffff)
            .boolean("reset", "Reset and clear RAM before loading.")
            .boolean("autoplay", "Start cassette transport; default true.")
            .build();
    }
    mcp::tool_result invoke(const json::value &arguments) override
    {
        const bool has_path = arguments["path"].is_string();
        const bool has_data = !arguments["data"].is_null();
        if (has_path == has_data)
            return mcp::tool_result::failure(
                "supply exactly one of path or data");

        std::vector<cpc::u8> bytes;
        std::string source = "inline data";
        if (has_path) {
            source = arguments["path"].as_string();
            std::string error;
            auto file = cpc::read_file(source, error);
            if (!file)
                return mcp::tool_result::failure(error);
            bytes = std::move(*file);
        } else {
            auto value = read_bytes(arguments["data"]);
            if (!value)
                return mcp::tool_result::failure("data is not a byte stream");
            bytes = std::move(*value);
        }

        std::string format = arguments["format"].as_string();
        if (format.empty())
            format = has_path ? extension(source) : "binary";
        if (arguments["reset"].as_bool(false))
            machine().reset(true);

        std::string error;
        cpc::u16 entry = machine().instruction_address();
        if (format == "binary") {
            const auto address = read_int_in(arguments["address"], 0,
                                             0xffff, 0x8000);
            if (!address || bytes.size() > 0x10000)
                return mcp::tool_result::failure(
                    "invalid address or binary larger than 64 KiB");
            machine().write_memory(static_cast<cpc::u16>(*address), bytes);
            if (!arguments["start"].is_null()) {
                const auto start = read_int_in(arguments["start"], 0, 0xffff);
                if (!start)
                    return mcp::tool_result::failure("invalid start address");
                entry = static_cast<cpc::u16>(*start);
                machine().jump(entry);
            }
        } else if (format == "amsdos" || format == "sna") {
            if (!machine().quickload(bytes, !arguments["start"].is_null(),
                                     error)) {
                return mcp::tool_result::failure(error);
            }
            entry = machine().instruction_address();
        } else if (format == "cdt") {
            if (!machine().tape().insert_cdt(
                    bytes, arguments["autoplay"].as_bool(true), error)) {
                return mcp::tool_result::failure(error);
            }
        } else if (format == "tap") {
            if (!machine().tape().insert_tap(
                    bytes, arguments["autoplay"].as_bool(true), error)) {
                return mcp::tool_result::failure(error);
            }
        } else if (format == "dsk") {
            if (!machine().insert_disk(bytes, error))
                return mcp::tool_result::failure(error);
        } else if (format == "rom") {
            const std::size_t wanted = machine().has_disk_drive()
                                           ? 3 * 0x4000
                                           : 2 * 0x4000;
            if (bytes.size() != wanted) {
                return mcp::tool_result::failure(
                    "ROM set must be OS+BASIC (32768 bytes) for CPC464 or "
                    "OS+BASIC+AMSDOS (49152 bytes) for CPC664/CPC6128");
            }
            const std::span<const cpc::u8> image(bytes);
            const auto os = image.subspan(0, 0x4000);
            const auto basic = image.subspan(0x4000, 0x4000);
            const auto amsdos = machine().has_disk_drive()
                                    ? image.subspan(0x8000, 0x4000)
                                    : std::span<const cpc::u8>{};
            if (!machine().load_roms(os, basic, amsdos, error))
                return mcp::tool_result::failure(error);
            entry = 0;
        } else {
            return mcp::tool_result::failure("unsupported format '" +
                                             format + "'");
        }

        json::value result = json::value::make_object();
        result.set("format", json::value(format));
        result.set("bytes", json::value(static_cast<std::int64_t>(
                                bytes.size())));
        result.set("source", json::value(source));
        result.set("entry", json::value(entry));
        return mcp::tool_result::of(
            "loaded " + std::to_string(bytes.size()) + " bytes as " +
                format + " from " + source,
            std::move(result));
    }
};

} // namespace

void register_load_tool(mcp::tool_registry &registry, cpc::machine &target)
{
    registry.add(std::make_unique<load_tool>(target));
}

} // namespace tools
