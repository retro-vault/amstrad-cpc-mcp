//
// AY generator timing and MCP WAV capture.
//
// The sample counter is driven by emulated CCLK ticks, so this suite checks
// both the chip waveform and the exact relationship between 4 MHz CPC time and
// host PCM. It also exercises the same audio tools registered by the server.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cstdint>
#include <vector>

#include "cpc/machine.h"
#include "mcp/tool_registry.h"
#include "test_support.h"
#include "tools/registration.h"

namespace {

void configure_tone_a(cpc::machine &target)
{
    target.set_ay_register(0, 8);
    target.set_ay_register(1, 0);
    target.set_ay_register(6, 1);
    target.set_ay_register(7, 0x3e);
    target.set_ay_register(8, 0x0f);
    target.set_ay_register(9, 0);
    target.set_ay_register(10, 0);
}

void test_generator_and_sample_clock()
{
    test::section("AY tone and 4 MHz sample clock");

    cpc::machine target;
    configure_tone_a(target);
    std::vector<std::int16_t> samples;
    target.set_audio_observer(
        8000, [&samples](std::int16_t sample) {
            samples.push_back(sample);
        });
    target.run_tstates(400000);
    target.clear_audio_observer();

    test::check_eq(samples.size(), 800,
                   "one tenth of CPC time produces exactly 800 samples");
    if (!samples.empty()) {
        const auto [low, high] =
            std::minmax_element(samples.begin(), samples.end());
        test::check(*low < *high, "tone A produces a changing waveform");
        test::check(*low < 0 && *high > 0,
                    "DC adjustment centers the waveform around zero");
    }
}

void test_mcp_playback_capture()
{
    test::section("MCP AY playback capture");

    cpc::machine target;
    mcp::tool_registry registry;
    tools::register_audio_tools(registry, target);
    mcp::tool *start = registry.find("audio_start");
    mcp::tool *stop = registry.find("audio_stop");
    test::check(start != nullptr, "audio_start is registered");
    test::check(stop != nullptr, "audio_stop is registered");
    if (!start || !stop)
        return;

    test::check(stop->invoke(json::value::make_object()).is_error,
                "stopping without a capture is rejected");

    json::value arguments = json::value::make_object();
    arguments.set("output", json::value("play"));
    arguments.set("sample_rate", json::value(8000));
    const mcp::tool_result started = start->invoke(arguments);
    test::check(!started.is_error, "buffered playback starts");
    test::check(start->invoke(arguments).is_error,
                "a second simultaneous capture is rejected");

    configure_tone_a(target);
    target.run_tstates(40000);
    const mcp::tool_result stopped =
        stop->invoke(json::value::make_object());
    test::check(!stopped.is_error, "buffered playback stops cleanly");
    test::check_eq(stopped.structured["samples"].as_int(), 80,
                   "MCP capture reports the exact emulated sample count");
    test::check_eq(stopped.structured["sample_rate"].as_int(), 8000,
                   "MCP capture retains its selected rate");

    const json::value wire = stopped.to_json();
    test::check_eq(wire["content"].size(), 2,
                   "the result contains text and audio blocks");
    test::check_eq_str(wire["content"].at(1)["type"].as_string(),
                       "audio", "the second content block is audio");
    test::check_eq_str(wire["content"].at(1)["mimeType"].as_string(),
                       "audio/wav", "playback is returned as WAV");
    test::check(wire["content"].at(1)["data"].as_string().starts_with(
                    "UklGR"),
                "base64 audio begins with a RIFF signature");
}

} // namespace

int main()
{
    test_generator_and_sample_clock();
    test_mcp_playback_capture();
    return test::summary("audio");
}
