#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace fdc {
using Clock = std::chrono::steady_clock;
struct Frame {
    uint16_t format = 0;
    uint32_t width = 0, height = 0;
    uint64_t sequence = 0, timestamp = 0;
    std::vector<uint8_t> pixels;
    Clock::time_point submitted;
};
size_t payload_size(uint16_t format, uint32_t width, uint32_t height);

// Only the background thread touches sockets. OBS callbacks never wait on IPC.
class Bridge {
public:
    Bridge();
    ~Bridge();
    Bridge(const Bridge &) = delete;
    Bridge &operator=(const Bridge &) = delete;
    void configure(bool enabled, uint16_t port, std::string token);
    void submit(Frame frame);
    std::optional<Frame> take(uint16_t format, uint32_t width, uint32_t height);
    std::string status() const;
private:
    void run();
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    bool stopping_ = false, enabled_ = false;
    uint16_t port_ = 0;
    std::string token_;
    uint64_t generation_ = 0, sequence_ = 0, round_trips_ = 0;
    std::optional<Frame> pending_, result_;
    std::string status_ = "IPC desativado";
    std::thread thread_;
};
} // namespace fdc
