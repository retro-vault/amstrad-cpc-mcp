//
// Assemble one persistent CPC, its tools and an MCP stdio server.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <charconv>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cpc/machine.h"
#include "json/writer.h"
#include "mcp/server.h"
#include "mcp/tool_registry.h"
#include "mcp/transport.h"
#include "tools/registration.h"

namespace {

constexpr const char *server_name = "amstrad-cpc-mcp";
constexpr const char *server_version = "1.0.0";
constexpr const char *cpm_disk_name = "cpm-2.2-en.dsk";

struct options {
    cpc::model model = cpc::model::cpc6128;
    cpc::crtc_type crtc = cpc::crtc_type::type_0;
    std::string os_rom;
    std::string basic_rom;
    std::string amsdos_rom;
    std::string load;
    bool verbose = false;
    bool list_tools = false;
    bool help = false;
    bool version = false;
    bool failed = false;
    std::string error;
};

void usage(std::ostream &out)
{
    out << "usage: " << server_name << " [options]\n\n"
        << "Cycle-stepped Amstrad CPC 464, 664 and 6128 MCP server.\n\n"
        << "  --model MODEL       cpc464, cpc664 or cpc6128\n"
        << "  --crtc TYPE         fitted CRTC type 0, 1 or 2\n"
        << "  --os-rom PATH       16 KiB model OS ROM\n"
        << "  --basic-rom PATH    16 KiB model BASIC ROM\n"
        << "  --amsdos-rom PATH   16 KiB AMSDOS ROM (664/6128)\n"
        << "  --load PATH         insert .dsk/.cdt/.tap or raw at 0x8000\n"
        << "  --list-tools        print MCP tool schemas and exit\n"
        << "  --verbose           log protocol activity to stderr\n"
        << "  --version           print version and exit\n"
        << "  --help              print this help and exit\n";
}

options parse(int argc, char **argv)
{
    options result;
    for (int i = 1; i < argc && !result.failed; ++i) {
        const std::string_view argument = argv[i];
        const auto value = [&](const char *name) -> std::string {
            if (i + 1 == argc) {
                result.failed = true;
                result.error = std::string(name) + " needs a value";
                return {};
            }
            return argv[++i];
        };
        if (argument == "--help" || argument == "-h") {
            result.help = true;
        } else if (argument == "--version") {
            result.version = true;
        } else if (argument == "--verbose" || argument == "-v") {
            result.verbose = true;
        } else if (argument == "--list-tools") {
            result.list_tools = true;
        } else if (argument == "--model") {
            const std::string name = value("--model");
            const auto model = cpc::model_from_name(name);
            if (!result.failed && !model) {
                result.failed = true;
                result.error = "model must be cpc464, cpc664 or cpc6128";
            } else if (model) {
                result.model = *model;
            }
        } else if (argument == "--crtc") {
            const std::string text = value("--crtc");
            int number = -1;
            const auto parsed = std::from_chars(text.data(),
                                                text.data() + text.size(),
                                                number);
            const auto crtc = cpc::crtc_type_from_number(number);
            if (!result.failed &&
                (parsed.ec != std::errc() ||
                 parsed.ptr != text.data() + text.size() || !crtc)) {
                result.failed = true;
                result.error = "CRTC type must be 0, 1 or 2";
            } else if (crtc) {
                result.crtc = *crtc;
            }
        } else if (argument == "--os-rom") {
            result.os_rom = value("--os-rom");
        } else if (argument == "--basic-rom") {
            result.basic_rom = value("--basic-rom");
        } else if (argument == "--amsdos-rom") {
            result.amsdos_rom = value("--amsdos-rom");
        } else if (argument == "--load") {
            result.load = value("--load");
        } else {
            result.failed = true;
            result.error = "unknown option '" + std::string(argument) + "'";
        }
    }
    return result;
}

bool load_firmware(cpc::machine &machine, const options &settings)
{
    const bool any = !settings.os_rom.empty() || !settings.basic_rom.empty() ||
                     !settings.amsdos_rom.empty();
    if (!any)
        return true;
    if (settings.os_rom.empty() || settings.basic_rom.empty() ||
        (machine.has_disk_drive() && settings.amsdos_rom.empty())) {
        std::cerr << server_name << ": supply the complete firmware set\n";
        return false;
    }
    std::string error;
    const auto os = cpc::read_file(settings.os_rom, error);
    if (!os) {
        std::cerr << server_name << ": " << error << '\n';
        return false;
    }
    const auto basic = cpc::read_file(settings.basic_rom, error);
    if (!basic) {
        std::cerr << server_name << ": " << error << '\n';
        return false;
    }
    std::vector<cpc::u8> empty;
    std::optional<std::vector<cpc::u8>> amsdos;
    if (machine.has_disk_drive()) {
        amsdos = cpc::read_file(settings.amsdos_rom, error);
        if (!amsdos) {
            std::cerr << server_name << ": " << error << '\n';
            return false;
        }
    }
    if (!machine.load_roms(*os, *basic, amsdos ? *amsdos : empty, error)) {
        std::cerr << server_name << ": " << error << '\n';
        return false;
    }
    return true;
}

bool load_startup(cpc::machine &machine, const std::string &path)
{
    if (path.empty())
        return true;
    std::string error;
    const auto data = cpc::read_file(path, error);
    if (!data) {
        std::cerr << server_name << ": " << error << '\n';
        return false;
    }
    std::string lower = path;
    for (char &value : lower)
        value = static_cast<char>(
            std::tolower(static_cast<unsigned char>(value)));
    bool ok = true;
    if (lower.ends_with(".dsk"))
        ok = machine.insert_disk(*data, error);
    else if (lower.ends_with(".cdt"))
        ok = machine.tape().insert_cdt(*data, true, error);
    else if (lower.ends_with(".tap"))
        ok = machine.tape().insert_tap(*data, true, error);
    else {
        machine.write_memory(0x8000, *data);
        machine.jump(0x8000);
    }
    if (!ok)
        std::cerr << server_name << ": " << error << '\n';
    return ok;
}

std::string default_cpm_disk(const char *program)
{
    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path source = fs::path("data") / "disks" / cpm_disk_name;
    if (fs::is_regular_file(source, error))
        return source.string();

    error.clear();
    fs::path executable = fs::read_symlink("/proc/self/exe", error);
    if (error) {
        error.clear();
        executable = fs::absolute(program, error);
    }
    if (!error) {
        const fs::path root = executable.parent_path().parent_path();
        const fs::path installed = root / "share" / server_name /
                                   "disks" / cpm_disk_name;
        if (fs::is_regular_file(installed, error))
            return installed.string();
    }
    return source.string();
}

} // namespace

int main(int argc, char **argv)
{
    const options settings = parse(argc, argv);
    if (settings.failed) {
        std::cerr << server_name << ": " << settings.error << "\n\n";
        usage(std::cerr);
        return 2;
    }
    if (settings.help) {
        usage(std::cout);
        return 0;
    }
    if (settings.version) {
        std::cout << server_name << ' ' << server_version << '\n';
        return 0;
    }

    cpc::machine_config config;
    config.machine_model = settings.model;
    config.fitted_crtc = settings.crtc;
    cpc::machine machine(config);
    mcp::tool_registry registry;
    tools::register_all_tools(registry, machine,
                              default_cpm_disk(argv[0]));
    if (settings.list_tools) {
        std::cout << json::write_pretty(registry.list()) << '\n';
        return 0;
    }
    if (!load_firmware(machine, settings) ||
        !load_startup(machine, settings.load)) {
        return 2;
    }

    mcp::server server({server_name, server_version}, registry);
    if (settings.verbose) {
        server.set_logger([](std::string_view line) {
            std::cerr << server_name << ": " << line << '\n';
        });
    }
    mcp::stdio_transport transport;
    server.run(transport);
    return 0;
}
