//
// Buffered raw-PCM delivery to a host audio player process.
//
// The producer writes emulated samples into a Unix-domain socket while the
// child reads that socket as standard input. The socket queue absorbs short
// pauses between MCP requests, and its backpressure makes the audio device
// rather than host scheduling pace continuous playback.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef TOOLS_LIVE_AUDIO_PLAYER_H
#define TOOLS_LIVE_AUDIO_PLAYER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tools {

class live_audio_player final {
public:
    // Construct an inactive player with no child process.
    live_audio_player();

    // Stop and reap an active player before releasing its resources.
    ~live_audio_player();

    // A live player owns a process and socket and cannot be copied.
    live_audio_player(const live_audio_player &) = delete;

    // A live player owns a process and socket and cannot be assigned.
    live_audio_player &operator=(const live_audio_player &) = delete;

    // Start a child process that reads signed 16-bit little-endian PCM from
    // standard input. `command` contains the executable followed by its
    // arguments; `queue_bytes` requests the kernel-side prebuffer capacity.
    bool start(const std::vector<std::string> &command,
               std::size_t queue_bytes, std::string &error);

    // Queue one signed 16-bit little-endian sample. This blocks only when the
    // playback queue is full, allowing the audio device to pace the producer.
    bool push(std::int16_t sample, std::string &error);

    // Flush queued samples, close the player's input, and wait for it to
    // drain. Returns false if delivery or the child process failed.
    bool stop(std::string &error);

    // Return whether a player process currently owns the PCM stream.
    bool active() const;

    // Return the number of raw PCM bytes accepted from the producer.
    std::uint64_t bytes() const;

private:
    bool flush(std::string &error);

    int socket_ = -1;
    int child_pid_ = -1;
    std::vector<std::uint8_t> buffer_;
    std::uint64_t bytes_ = 0;
};

} // namespace tools

#endif // TOOLS_LIVE_AUDIO_PLAYER_H
