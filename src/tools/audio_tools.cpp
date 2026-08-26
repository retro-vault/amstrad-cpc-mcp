//
// MCP capture tools for the mixed CPC AY-3-8912 output.
//
// Audio is sampled from emulated T-states rather than wall time and encoded
// as dependency-free, mono 16-bit PCM WAV. A capture can either be returned
// as an MCP audio block for playback or streamed to a caller-selected file.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "tools/audio_pacer.h"
#include "tools/live_audio_player.h"
#include "tools/registration.h"
#include "tools/support.h"

namespace tools {

namespace {

constexpr int default_sample_rate = 44100;
constexpr int minimum_sample_rate = 8000;
constexpr int maximum_sample_rate = 48000;
constexpr int maximum_play_seconds = 60;
constexpr int minimum_speed = 10;
constexpr int maximum_speed = 400;
constexpr int default_live_buffer_ms = 5000;
constexpr int minimum_live_buffer_ms = 100;
constexpr int maximum_live_buffer_ms = 10000;

enum class audio_destination {
    play,
    file,
};

// Append a little-endian integer to a byte buffer.
void append_le16(std::vector<cpc::u8> &bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<cpc::u8>(value & 0xff));
    bytes.push_back(static_cast<cpc::u8>((value >> 8) & 0xff));
}

void append_le32(std::vector<cpc::u8> &bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<cpc::u8>(value & 0xff));
    bytes.push_back(static_cast<cpc::u8>((value >> 8) & 0xff));
    bytes.push_back(static_cast<cpc::u8>((value >> 16) & 0xff));
    bytes.push_back(static_cast<cpc::u8>((value >> 24) & 0xff));
}

void append_tag(std::vector<cpc::u8> &bytes, std::string_view tag)
{
    for (char value : tag)
        bytes.push_back(static_cast<cpc::u8>(value));
}

// Build the fixed PCM WAV header for a known number of mono samples.
std::vector<cpc::u8> wav_header(int sample_rate,
                                     cpc::u64 samples)
{
    const auto data_bytes = static_cast<std::uint32_t>(samples * 2);
    std::vector<cpc::u8> bytes;
    bytes.reserve(44);
    append_tag(bytes, "RIFF");
    append_le32(bytes, 36 + data_bytes);
    append_tag(bytes, "WAVE");
    append_tag(bytes, "fmt ");
    append_le32(bytes, 16);
    append_le16(bytes, 1);
    append_le16(bytes, 1);
    append_le32(bytes, static_cast<std::uint32_t>(sample_rate));
    append_le32(bytes, static_cast<std::uint32_t>(sample_rate * 2));
    append_le16(bytes, 2);
    append_le16(bytes, 16);
    append_tag(bytes, "data");
    append_le32(bytes, data_bytes);
    return bytes;
}

bool usable_path(const std::string &path)
{
    return !path.empty() && path.find('\0') == std::string::npos;
}

// An active capture shared by the start and stop tool objects.
class audio_session {
public:
    ~audio_session()
    {
        if (active_) {
            std::string ignored;
            (void)stop(ignored);
        }
    }

    bool start(audio_destination destination, const std::string &path,
               int sample_rate, int speed, bool live,
               const std::string &device, int buffer_ms,
               std::string &error)
    {
        if (active_) {
            error = "an audio capture is already active";
            return false;
        }
        if (destination == audio_destination::file && !usable_path(path)) {
            error = "'path' is required for file audio output";
            return false;
        }

        destination_ = destination;
        path_ = destination == audio_destination::file ? path : "";
        sample_rate_ = sample_rate;
        speed_ = speed;
        live_ = live;
        samples_ = 0;
        truncated_ = false;
        error_.clear();
        playback_samples_.clear();
        pacer_.configure(sample_rate_, live_ ? 0 : speed_);

        if (destination_ == audio_destination::file) {
            file_.clear();
            file_.open(path_, std::ios::binary | std::ios::trunc);
            if (!file_) {
                error = "cannot open '" + path_ + "' for audio output";
                return false;
            }
            const std::vector<cpc::u8> header =
                wav_header(sample_rate_, 0);
            file_.write(reinterpret_cast<const char *>(header.data()),
                        static_cast<std::streamsize>(header.size()));
            if (!file_) {
                file_.close();
                error = "cannot write the WAV header to '" + path_ + "'";
                return false;
            }
        } else if (!live_) {
            playback_samples_.reserve(
                static_cast<std::size_t>(sample_rate_) * 5);
        }

        if (live_) {
            const int playback_rate = sample_rate_ * speed_ / 100;
            const std::vector<std::string> command = {
                "aplay", "-q", "-D", device, "-t", "raw",
                "-f", "S16_LE", "--buffer-time=500000",
                "--period-time=20000",
                "-c", "1", "-r", std::to_string(playback_rate),
            };
            const auto queue_bytes = static_cast<std::size_t>(
                playback_rate) * 2u * buffer_ms / 1000u;
            if (!live_player_.start(command, queue_bytes, error))
                return false;
        }

        active_ = true;
        error.clear();
        return true;
    }

    void capture(std::int16_t sample)
    {
        if (!active_ || !error_.empty())
            return;

        if (live_) {
            if (!live_player_.push(sample, error_))
                return;
            ++samples_;
            return;
        }

        if (destination_ == audio_destination::play) {
            const auto limit = static_cast<cpc::u64>(sample_rate_) *
                               maximum_play_seconds;
            if (samples_ >= limit) {
                truncated_ = true;
                pacer_.wait();
                return;
            }
            playback_samples_.push_back(sample);
            ++samples_;
            pacer_.wait();
            return;
        }

        constexpr cpc::u64 maximum_wav_samples =
            (std::numeric_limits<std::uint32_t>::max() - 36u) / 2u;
        if (samples_ >= maximum_wav_samples) {
            error_ = "WAV output exceeds the format's 4 GB size limit";
            return;
        }

        const auto raw = static_cast<std::uint16_t>(sample);
        const char bytes[2] = {
            static_cast<char>(raw & 0xff),
            static_cast<char>((raw >> 8) & 0xff),
        };
        file_.write(bytes, sizeof bytes);
        if (!file_) {
            error_ = "a write failed while recording '" + path_ + "'";
            return;
        }
        ++samples_;
        pacer_.wait();
    }

    bool stop(std::string &error)
    {
        if (!active_) {
            error = "no audio capture is active";
            return false;
        }

        active_ = false;
        if (live_) {
            std::string player_error;
            if (!live_player_.stop(player_error) && error_.empty())
                error_ = player_error;
        }
        if (destination_ == audio_destination::file && error_.empty()) {
            const std::vector<cpc::u8> header =
                wav_header(sample_rate_, samples_);
            file_.seekp(0);
            file_.write(reinterpret_cast<const char *>(header.data()),
                        static_cast<std::streamsize>(header.size()));
            if (!file_)
                error_ = "cannot finish WAV output '" + path_ + "'";
        }

        if (file_.is_open()) {
            file_.close();
            if (file_.fail() && error_.empty())
                error_ = "cannot close WAV output '" + path_ + "'";
        }

        if (!error_.empty()) {
            error = error_;
            return false;
        }
        error.clear();
        return true;
    }

    std::vector<cpc::u8> playback_wav() const
    {
        std::vector<cpc::u8> bytes = wav_header(sample_rate_, samples_);
        bytes.reserve(bytes.size() + playback_samples_.size() * 2);
        for (std::int16_t sample : playback_samples_)
            append_le16(bytes, static_cast<std::uint16_t>(sample));
        return bytes;
    }

    bool active() const { return active_; }
    audio_destination destination() const { return destination_; }
    const std::string &path() const { return path_; }
    int sample_rate() const { return sample_rate_; }
    int speed() const { return speed_; }
    bool live() const { return live_; }
    cpc::u64 samples() const { return samples_; }
    bool truncated() const { return truncated_; }
    cpc::u64 bytes() const
    {
        return (live_ ? 0 : 44) + samples_ * 2;
    }

private:
    std::ofstream file_;
    audio_pacer pacer_;
    live_audio_player live_player_;
    std::vector<std::int16_t> playback_samples_;
    std::string path_;
    std::string error_;
    audio_destination destination_ = audio_destination::play;
    int sample_rate_ = default_sample_rate;
    int speed_ = 0;
    cpc::u64 samples_ = 0;
    bool active_ = false;
    bool truncated_ = false;
    bool live_ = false;
};

class audio_start_tool final : public machine_tool {
public:
    audio_start_tool(cpc::machine &target,
                     std::shared_ptr<audio_session> session)
        : machine_tool(target), session_(std::move(session))
    {
    }

    std::string name() const override { return "audio_start"; }

    std::string description() const override
    {
        return "Start capturing the CPC AY-3-8912 mix. Choose 'play' "
               "to receive playable WAV audio from audio_stop, or 'file' "
               "to save it to a path. With play, live streams through the "
               "host audio device using a buffered audio clock. Optional "
               "speed 100 is real CPC speed.";
    }

    json::value input_schema() const override
    {
        return schema_builder()
            .string("output", "'play' returns an MCP audio block; 'file' "
                              "writes a WAV file. Default 'play'.",
                    {"play", "file"})
            .string("path", "Destination WAV path. Required for 'file'.")
            .boolean("live", "For 'play', stream through the host audio "
                             "device while the machine runs instead of "
                             "returning a WAV from audio_stop. Requires "
                             "aplay. Default false.")
            .string("device", "ALSA PCM device for live playback. Default "
                              "'default'; for example 'pipewire', 'pulse', "
                              "or 'hw:0,0'.")
            .integer("buffer_ms", "Queued live audio in milliseconds, 100 "
                                  "to 10000. Default 5000. Increase this "
                                  "only for unusually slow MCP round trips.",
                     minimum_live_buffer_ms, maximum_live_buffer_ms)
            .integer("sample_rate", "Samples per second, 8000 to 48000. "
                                    "Default 44100.",
                     minimum_sample_rate, maximum_sample_rate)
            .integer("speed", "Optional emulation speed percentage, 10 to "
                              "400. 100 is real CPC speed. Live playback "
                              "defaults to 100; other modes run unthrottled "
                              "when omitted.",
                     minimum_speed, maximum_speed)
            .build();
    }

    mcp::tool_result invoke(const json::value &arguments) override
    {
        const std::string output = arguments["output"].as_string("play");
        audio_destination destination;
        if (output == "play") {
            destination = audio_destination::play;
        } else if (output == "file") {
            destination = audio_destination::file;
        } else {
            return mcp::tool_result::failure(
                "'output' must be 'play' or 'file'");
        }

        const bool live = arguments["live"].as_bool(false);
        if (live && destination != audio_destination::play)
            return mcp::tool_result::failure(
                "'live' is available only when 'output' is 'play'");
        const std::string device =
            arguments["device"].as_string("default");
        if (live && !usable_path(device))
            return mcp::tool_result::failure(
                "'device' must name an ALSA PCM device");
        const auto buffer_ms = read_int_in(
            arguments["buffer_ms"], minimum_live_buffer_ms,
            maximum_live_buffer_ms, default_live_buffer_ms);
        if (!buffer_ms)
            return mcp::tool_result::failure(
                "'buffer_ms' must be between 100 and 10000");
        if (!live && !arguments["buffer_ms"].is_null())
            return mcp::tool_result::failure(
                "'buffer_ms' is available only for live playback");

        const auto sample_rate = read_int_in(
            arguments["sample_rate"], minimum_sample_rate,
            maximum_sample_rate, default_sample_rate);
        if (!sample_rate)
            return mcp::tool_result::failure(
                "'sample_rate' must be between 8000 and 48000");

        int speed = 0;
        if (!arguments["speed"].is_null()) {
            const auto requested_speed = read_int_in(
                arguments["speed"], minimum_speed, maximum_speed);
            if (!requested_speed)
                return mcp::tool_result::failure(
                    "'speed' must be between 10 and 400 percent");
            speed = static_cast<int>(*requested_speed);
        }
        if (live && speed == 0)
            speed = 100;

        const std::string path = arguments["path"].as_string();
        std::string error;
        if (!session_->start(destination, path,
                             static_cast<int>(*sample_rate), speed, live,
                             device, static_cast<int>(*buffer_ms), error)) {
            return mcp::tool_result::failure(error);
        }

        const std::shared_ptr<audio_session> session = session_;
        machine().set_audio_observer(
            static_cast<int>(*sample_rate),
            [session](std::int16_t sample) { session->capture(sample); });

        json::value structured = json::value::make_object();
        structured.set("output", json::value(output));
        structured.set("live", json::value(live));
        structured.set("sample_rate", json::value(*sample_rate));
        if (speed != 0)
            structured.set("speed", json::value(speed));
        structured.set("channels", json::value(1));
        structured.set("bits_per_sample", json::value(16));
        if (live)
            structured.set("device", json::value(device));
        if (live)
            structured.set("buffer_ms", json::value(*buffer_ms));
        if (destination == audio_destination::file)
            structured.set("path", json::value(path));

        const std::string destination_text =
            live
                ? "through the host audio device"
                : destination == audio_destination::play
                ? "for MCP playback after audio_stop"
                : "to '" + path + "'";
        return mcp::tool_result::of(
            "capturing CPC AY audio at " +
                std::to_string(*sample_rate) + " Hz " + destination_text +
                (speed == 0 ? " without wall-clock throttling"
                            : " at " + std::to_string(speed) + "% speed"),
            std::move(structured));
    }

private:
    std::shared_ptr<audio_session> session_;
};

class audio_stop_tool final : public machine_tool {
public:
    audio_stop_tool(cpc::machine &target,
                    std::shared_ptr<audio_session> session)
        : machine_tool(target), session_(std::move(session))
    {
    }

    std::string name() const override { return "audio_stop"; }

    std::string description() const override
    {
        return "Stop the active AY capture. A 'play' capture returns a "
               "WAV audio content block, while live play drains and closes "
               "the host player. A 'file' capture closes the WAV and "
               "returns its path. Buffered playback captures are limited "
               "to 60 seconds.";
    }

    json::value input_schema() const override
    {
        return schema_builder().build();
    }

    mcp::tool_result invoke(const json::value &) override
    {
        if (!session_->active())
            return mcp::tool_result::failure(
                "no audio capture is active");

        machine().clear_audio_observer();
        std::string error;
        if (!session_->stop(error))
            return mcp::tool_result::failure(error);

        const double duration =
            static_cast<double>(session_->samples()) /
            session_->sample_rate();
        json::value structured = json::value::make_object();
        structured.set("output", json::value(
            session_->destination() == audio_destination::play
                ? "play" : "file"));
        structured.set("live", json::value(session_->live()));
        structured.set("format", json::value(
            session_->live() ? "pcm-s16le-mono"
                             : "wav-pcm-s16le-mono"));
        structured.set("sample_rate", json::value(session_->sample_rate()));
        if (session_->speed() != 0)
            structured.set("speed", json::value(session_->speed()));
        structured.set("channels", json::value(1));
        structured.set("bits_per_sample", json::value(16));
        structured.set("samples", json::value(session_->samples()));
        structured.set("duration_seconds", json::value(duration));
        structured.set("bytes", json::value(session_->bytes()));
        structured.set("truncated", json::value(session_->truncated()));

        if (session_->live()) {
            return mcp::tool_result::of(
                "played " + std::to_string(session_->samples()) +
                    " AY samples through the host audio device",
                std::move(structured));
        }

        if (session_->destination() == audio_destination::file) {
            structured.set("path", json::value(session_->path()));
            return mcp::tool_result::of(
                "saved " + std::to_string(session_->samples()) +
                    " AY samples to '" + session_->path() + "'",
                std::move(structured));
        }

        const std::vector<cpc::u8> wav = session_->playback_wav();
        const std::string_view wav_view(
            reinterpret_cast<const char *>(wav.data()), wav.size());
        mcp::tool_result result;
        result.content.push_back(mcp::content_block::text(
            "captured " + std::to_string(session_->samples()) +
            " AY samples for playback" +
            (session_->truncated() ? " (limited to 60 seconds)" : "")));
        result.content.push_back(mcp::content_block::audio(
            mcp::base64_encode(wav_view), "audio/wav"));
        result.structured = std::move(structured);
        return result;
    }

private:
    std::shared_ptr<audio_session> session_;
};

} // namespace

void register_audio_tools(mcp::tool_registry &registry,
                          cpc::machine &target)
{
    auto session = std::make_shared<audio_session>();
    registry.add(std::make_unique<audio_start_tool>(target, session));
    registry.add(std::make_unique<audio_stop_tool>(target, session));
}

} // namespace tools
