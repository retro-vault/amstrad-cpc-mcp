//
// MCP handshake, discovery and representative CPC tool calls.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <memory>
#include <string>

#include "cpc/machine.h"
#include "json/parser.h"
#include "json/writer.h"
#include "mcp/server.h"
#include "tools/registration.h"
#include "test_support.h"

namespace {

struct harness {
    cpc::machine machine;
    mcp::tool_registry registry;
    mcp::server server;

    harness() : server({"test-cpc", "1"}, registry)
    {
        tools::register_all_tools(registry, machine);
    }

    json::value send(json::value message)
    {
        const auto reply = server.handle_message(json::write(message));
        if (!reply)
            return {};
        const auto parsed = json::parse(*reply);
        return parsed.ok ? parsed.document : json::value{};
    }

    json::value request(int id, const std::string &method,
                        json::value params = {})
    {
        json::value value = json::value::make_object();
        value.set("jsonrpc", json::value("2.0"));
        value.set("id", json::value(id));
        value.set("method", json::value(method));
        if (!params.is_null())
            value.set("params", std::move(params));
        return send(std::move(value));
    }

    json::value call(int id, const std::string &name, json::value arguments)
    {
        json::value params = json::value::make_object();
        params.set("name", json::value(name));
        params.set("arguments", std::move(arguments));
        return request(id, "tools/call", std::move(params));
    }
};

void test_protocol_and_tools()
{
    test::section("MCP handshake and CPC tools");
    harness probe;
    json::value init = json::value::make_object();
    init.set("protocolVersion", json::value("2025-06-18"));
    init.set("capabilities", json::value::make_object());
    const json::value hello = probe.request(1, "initialize", std::move(init));
    test::check_eq_str(hello["result"]["serverInfo"]["name"].as_string(),
                       "test-cpc", "server identifies itself");
    const json::value listed = probe.request(2, "tools/list");
    test::check(listed["result"]["tools"].size() >= 20,
                "complete CPC tool set is discoverable");
    const std::string schemas = json::write(listed);
    test::check(schemas.find("\"cdt\"") != std::string::npos,
                "load schema exposes the CPC CDT format");
    test::check(schemas.find("\"name\":\"cpm\"") != std::string::npos,
                "CP/M boot workflow is discoverable");

    test::check(probe.call(3, "cpm", json::value::make_object())["result"]
                    ["isError"].as_bool(false),
                "CP/M boot refuses the development stub firmware");

    const json::value status = probe.call(
        4, "status", json::value::make_object());
    test::check_eq_str(status["result"]["structuredContent"]["model"]
                           .as_string(),
                       "cpc6128", "status reports the selected model");
    test::check_eq(status["result"]["structuredContent"]
                       ["tstates_per_frame"].as_int(),
                   79872, "status reports CPC frame timing");

    json::value load = json::value::make_object();
    load.set("data", json::value("3e5a32009076"));
    load.set("format", json::value("binary"));
    load.set("address", json::value(0x8000));
    load.set("start", json::value(0x8000));
    test::check(!probe.call(5, "load", std::move(load))["result"]
                     ["isError"].as_bool(false),
                "inline machine code loads");

    json::value port = json::value::make_object();
    port.set("port", json::value(0x7f00));
    port.set("value", json::value(0x8c));
    probe.call(6, "write_port", std::move(port));
    json::value run = json::value::make_object();
    run.set("instructions", json::value(3));
    probe.call(7, "run", std::move(run));
    test::check_eq(probe.machine.read_memory(0x9000), 0x5a,
                   "MCP-run code changed CPC memory");

    json::value screen_arguments = json::value::make_object();
    screen_arguments.set("monitor", json::value("green"));
    const json::value screen = probe.call(
        8, "screen", std::move(screen_arguments));
    test::check_eq_str(screen["result"]["content"].at(1)["type"].as_string(),
                       "image", "screen returns a PNG image block");
    test::check_eq_str(screen["result"]["structuredContent"]["monitor"]
                           .as_string(),
                       "green", "screen reports the selected GT64 monitor");

    json::value bad_monitor = json::value::make_object();
    bad_monitor.set("monitor", json::value("amber"));
    test::check(probe.call(9, "screen", std::move(bad_monitor))["result"]
                    ["isError"].as_bool(false),
                "screen rejects an unsupported monitor");
}

} // namespace

int main()
{
    test_protocol_and_tools();
    return test::summary("mcp");
}
