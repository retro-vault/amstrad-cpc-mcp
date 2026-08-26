//
// Buffered raw-PCM delivery to a host audio player process.
//
// A socket pair supplies both a useful kernel queue and MSG_NOSIGNAL writes,
// so closing or rejecting the audio device becomes a tool error rather than
// terminating the MCP server with SIGPIPE.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "tools/live_audio_player.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace tools {

namespace {

constexpr std::size_t pcm_chunk_bytes = 4096;

bool set_close_on_exec(int descriptor)
{
    const int flags = ::fcntl(descriptor, F_GETFD);
    return flags >= 0 &&
           ::fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) >= 0;
}

void report_child_error(int descriptor, int error)
{
    const auto *bytes = reinterpret_cast<const char *>(&error);
    std::size_t offset = 0;
    while (offset < sizeof error) {
        const ssize_t written =
            ::write(descriptor, bytes + offset, sizeof error - offset);
        if (written > 0) {
            offset += static_cast<std::size_t>(written);
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
}

} // namespace

live_audio_player::live_audio_player()
{
    buffer_.reserve(pcm_chunk_bytes);
}

live_audio_player::~live_audio_player()
{
    if (active()) {
        std::string ignored;
        (void)stop(ignored);
    }
}

bool live_audio_player::start(const std::vector<std::string> &command,
                              std::size_t queue_bytes,
                              std::string &error)
{
    if (active()) {
        error = "a live audio player is already active";
        return false;
    }
    if (command.empty() || command.front().empty()) {
        error = "the live audio player command is empty";
        return false;
    }

    int stream[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, stream) < 0) {
        error = "cannot create the live audio buffer: " +
                std::string(std::strerror(errno));
        return false;
    }

    int status_pipe[2];
    if (::pipe(status_pipe) < 0) {
        const int saved = errno;
        ::close(stream[0]);
        ::close(stream[1]);
        error = "cannot create the audio player status pipe: " +
                std::string(std::strerror(saved));
        return false;
    }

    if (!set_close_on_exec(stream[0]) ||
        !set_close_on_exec(stream[1]) ||
        !set_close_on_exec(status_pipe[0]) ||
        !set_close_on_exec(status_pipe[1])) {
        const int saved = errno;
        ::close(stream[0]);
        ::close(stream[1]);
        ::close(status_pipe[0]);
        ::close(status_pipe[1]);
        error = "cannot configure the live audio descriptors: " +
                std::string(std::strerror(saved));
        return false;
    }

    const std::size_t bounded_queue = std::min(
        queue_bytes,
        static_cast<std::size_t>(std::numeric_limits<int>::max()));
    const int socket_buffer = static_cast<int>(bounded_queue);
    if (socket_buffer > 0) {
        (void)::setsockopt(stream[0], SOL_SOCKET, SO_SNDBUF,
                           &socket_buffer, sizeof socket_buffer);
    }

    std::vector<char *> arguments;
    arguments.reserve(command.size() + 1);
    for (const std::string &part : command)
        arguments.push_back(const_cast<char *>(part.c_str()));
    arguments.push_back(nullptr);

    const pid_t child = ::fork();
    if (child < 0) {
        const int saved = errno;
        ::close(stream[0]);
        ::close(stream[1]);
        ::close(status_pipe[0]);
        ::close(status_pipe[1]);
        error = "cannot start the live audio player: " +
                std::string(std::strerror(saved));
        return false;
    }

    if (child == 0) {
        ::close(stream[0]);
        ::close(status_pipe[0]);
        if (::dup2(stream[1], STDIN_FILENO) < 0) {
            report_child_error(status_pipe[1], errno);
            ::_exit(127);
        }
        ::close(stream[1]);

        const int null_output = ::open("/dev/null", O_WRONLY);
        if (null_output < 0 ||
            ::dup2(null_output, STDOUT_FILENO) < 0) {
            report_child_error(status_pipe[1], errno);
            ::_exit(127);
        }
        ::close(null_output);

        ::execvp(arguments[0], arguments.data());
        report_child_error(status_pipe[1], errno);
        ::_exit(127);
    }

    ::close(stream[1]);
    ::close(status_pipe[1]);

    int child_error = 0;
    ssize_t status_bytes;
    do {
        status_bytes = ::read(status_pipe[0], &child_error,
                              sizeof child_error);
    } while (status_bytes < 0 && errno == EINTR);
    const int status_error = errno;
    ::close(status_pipe[0]);

    if (status_bytes != 0) {
        ::close(stream[0]);
        int status;
        while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
        }
        const int reported = status_bytes > 0 ? child_error : status_error;
        error = "cannot execute live audio player '" + command.front() +
                "': " + std::string(std::strerror(reported));
        return false;
    }

    socket_ = stream[0];
    child_pid_ = static_cast<int>(child);
    buffer_.clear();
    bytes_ = 0;
    error.clear();
    return true;
}

bool live_audio_player::push(std::int16_t sample, std::string &error)
{
    if (!active()) {
        error = "no live audio player is active";
        return false;
    }

    const auto raw = static_cast<std::uint16_t>(sample);
    buffer_.push_back(static_cast<std::uint8_t>(raw & 0xff));
    buffer_.push_back(static_cast<std::uint8_t>((raw >> 8) & 0xff));
    bytes_ += 2;
    if (buffer_.size() < pcm_chunk_bytes) {
        error.clear();
        return true;
    }
    return flush(error);
}

bool live_audio_player::flush(std::string &error)
{
    std::size_t offset = 0;
    while (offset < buffer_.size()) {
        const ssize_t sent = ::send(
            socket_, buffer_.data() + offset, buffer_.size() - offset,
            MSG_NOSIGNAL);
        if (sent > 0) {
            offset += static_cast<std::size_t>(sent);
        } else if (sent < 0 && errno == EINTR) {
            continue;
        } else {
            error = "live audio playback failed: " +
                    std::string(std::strerror(
                        sent == 0 ? EPIPE : errno));
            buffer_.clear();
            return false;
        }
    }
    buffer_.clear();
    error.clear();
    return true;
}

bool live_audio_player::stop(std::string &error)
{
    if (!active()) {
        error = "no live audio player is active";
        return false;
    }

    std::string delivery_error;
    const bool delivered = flush(delivery_error);
    (void)::shutdown(socket_, SHUT_WR);
    ::close(socket_);
    socket_ = -1;

    int status = 0;
    pid_t waited;
    do {
        waited = ::waitpid(static_cast<pid_t>(child_pid_), &status, 0);
    } while (waited < 0 && errno == EINTR);
    child_pid_ = -1;

    if (!delivered) {
        error = delivery_error;
        return false;
    }
    if (waited < 0) {
        error = "cannot wait for the live audio player: " +
                std::string(std::strerror(errno));
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        error = "the live audio player exited unsuccessfully";
        return false;
    }

    error.clear();
    return true;
}

bool live_audio_player::active() const
{
    return socket_ >= 0 && child_pid_ >= 0;
}

std::uint64_t live_audio_player::bytes() const
{
    return bytes_;
}

} // namespace tools
