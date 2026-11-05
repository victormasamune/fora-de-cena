#include "ipc-bridge.hpp"
#include "native-planes.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <thread>

int main(int argc, char **argv)
{
    if (argc != 5) return 2;
    const auto port = static_cast<uint16_t>(std::stoi(argv[1]));
    const std::string token = argv[2], mode = argv[3];
    const auto format = static_cast<uint16_t>(std::stoi(argv[4]));
    // Check row padding is neither sent nor overwritten.
    std::vector<uint8_t> padded{1, 2, 99, 99, 3, 4, 99, 99};
    std::array<fdc::Plane, 3> planes{};
    planes[0] = {padded.data(), 4, 2, 2};
    assert(fdc::pack_planes(planes, 4) == std::vector<uint8_t>({1, 2, 3, 4}));
    fdc::unpack_planes({5, 6, 7, 8}, planes);
    assert(padded == std::vector<uint8_t>({5, 6, 99, 99, 7, 8, 99, 99}));

    fdc::Bridge bridge;
    bridge.configure(true, port, token);
    fdc::Frame frame;
    frame.format = format; frame.width = 64; frame.height = 48; frame.timestamp = 987654321;
    frame.pixels.resize(fdc::payload_size(format, frame.width, frame.height));
    for (size_t i = 0; i < frame.pixels.size(); ++i) frame.pixels[i] = static_cast<uint8_t>(i % 251);
    if (mode == "stale" || mode == "reset" || mode == "resize") {
        bridge.submit(frame);
        const auto limit = fdc::Clock::now() + std::chrono::seconds(2);
        while (bridge.status().find("IPC conectado") == std::string::npos) {
            if (fdc::Clock::now() > limit) return 8;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (mode == "stale") std::this_thread::sleep_for(std::chrono::milliseconds(300));
        if (mode == "reset") bridge.configure(false, port, token);
        if (bridge.take(format, frame.width + (mode == "resize" ? 2 : 0), frame.height)) return 9;
        std::cout << "FALLBACK_OK\n";
        return 0;
    }
    const auto start = fdc::Clock::now();
    size_t successes = 0;
    while (fdc::Clock::now() - start < std::chrono::milliseconds(mode == "echo" ? 3000 : 1200)) {
        const auto before = fdc::Clock::now();
        bridge.submit(frame);
        auto reply = bridge.take(format, frame.width, frame.height);
        // No socket connect/read/write can block submit()/take().
        if (fdc::Clock::now() - before > std::chrono::milliseconds(100)) return 3;
        if (reply) {
            if (mode != "echo") return 4;
            if (reply->pixels != frame.pixels || reply->timestamp != frame.timestamp) return 5;
            if (++successes >= 3) {
                bridge.configure(false, port, token);
                if (bridge.take(format, frame.width, frame.height)) return 6;
                std::cout << "IPC_OK format=" << format << " " << successes << " round-trips\n";
                return 0;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (mode == "echo") { std::cerr << bridge.status() << '\n'; return 7; }
    std::cout << "FALLBACK_OK\n";
    return 0;
}
