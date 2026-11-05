#pragma once
#include "ipc-bridge.hpp"
#include <array>
#include <cstring>
#include <stdexcept>

namespace fdc {
struct Plane {
    uint8_t *data = nullptr;
    size_t stride = 0, row_bytes = 0, rows = 0;
};
// Never send padding, which may contain unrelated/uninitialized source memory.
inline std::vector<uint8_t> pack_planes(const std::array<Plane, 3> &planes, size_t size)
{
    std::vector<uint8_t> bytes(size);
    size_t offset = 0;
    for (const auto &plane : planes) {
        if (!plane.rows) continue;
        if (!plane.data || plane.stride < plane.row_bytes || plane.rows * plane.row_bytes > size - offset)
            throw std::runtime_error("Plano de vídeo inválido");
        for (size_t row = 0; row < plane.rows; ++row) {
            std::memcpy(bytes.data() + offset, plane.data + row * plane.stride, plane.row_bytes);
            offset += plane.row_bytes;
        }
    }
    if (offset != size) throw std::runtime_error("Planos de vídeo incompletos");
    return bytes;
}
inline void unpack_planes(const std::vector<uint8_t> &bytes, const std::array<Plane, 3> &planes)
{
    size_t expected = 0;
    for (const auto &plane : planes) {
        if (!plane.rows) continue;
        if (!plane.data || plane.stride < plane.row_bytes) throw std::runtime_error("Plano de vídeo inválido");
        expected += plane.rows * plane.row_bytes;
    }
    if (expected != bytes.size()) throw std::runtime_error("Planos de vídeo incompletos");
    size_t offset = 0;
    for (const auto &plane : planes) {
        for (size_t row = 0; row < plane.rows; ++row) {
            std::memcpy(plane.data + row * plane.stride, bytes.data() + offset, plane.row_bytes);
            offset += plane.row_bytes;
        }
    }
}
} // namespace fdc
