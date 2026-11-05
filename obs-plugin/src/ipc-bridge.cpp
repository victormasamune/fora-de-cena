#include "ipc-bridge.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fdc {
size_t payload_size(uint16_t format, uint32_t width, uint32_t height)
{
    if (width < 2 || width > 3840 || height < 2 || height > 2160)
        throw std::runtime_error("Dimensões IPC não suportadas");
    size_t area = static_cast<size_t>(width) * height;
    if (format >= 1 && format <= 3) return area * 4;
    if ((format == 4 || format == 5) && !(width % 2) && !(height % 2)) return area * 3 / 2;
    if (format >= 6 && format <= 8 && !(width % 2)) return area * 2;
    throw std::runtime_error("Formato IPC não suportado");
}

namespace {
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
void close_socket(Socket socket) { if (socket != invalid_socket) closesocket(socket); }
int socket_error() { return WSAGetLastError(); }
bool would_block(int error) { return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS; }
struct SocketRuntime {
    SocketRuntime() { WSADATA data{}; if (WSAStartup(MAKEWORD(2, 2), &data)) throw std::runtime_error("Winsock indisponível"); }
    ~SocketRuntime() { WSACleanup(); }
};
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
void close_socket(Socket socket) { if (socket != invalid_socket) close(socket); }
int socket_error() { return errno; }
bool would_block(int error) { return error == EAGAIN || error == EWOULDBLOCK || error == EINPROGRESS; }
struct SocketRuntime {};
#endif
struct Connection {
    Socket socket = invalid_socket;
    ~Connection() { reset(); }
    void reset() { close_socket(socket); socket = invalid_socket; }
};

void ready(Socket socket, bool writing, Clock::time_point deadline)
{
    auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - Clock::now()).count();
    if (remaining <= 0) throw std::runtime_error("Timeout IPC");
    timeval timeout{};
    timeout.tv_sec = static_cast<long>(remaining / 1000000);
    timeout.tv_usec = static_cast<long>(remaining % 1000000);
    fd_set read_set, write_set, error_set;
    FD_ZERO(&read_set); FD_ZERO(&write_set); FD_ZERO(&error_set);
    if (writing) FD_SET(socket, &write_set); else FD_SET(socket, &read_set);
    FD_SET(socket, &error_set);
#ifdef _WIN32
    int count = select(0, &read_set, &write_set, &error_set, &timeout);
#else
    int count = select(socket + 1, &read_set, &write_set, &error_set, &timeout);
#endif
    if (count <= 0 || FD_ISSET(socket, &error_set)) throw std::runtime_error("Conexão IPC sem resposta");
}

void transfer(Socket socket, uint8_t *bytes, size_t size, bool writing, Clock::time_point deadline)
{
    while (size) {
        ready(socket, writing, deadline);
        int amount = static_cast<int>(std::min<size_t>(size, 1024 * 1024));
#ifdef _WIN32
        int count = writing ? send(socket, reinterpret_cast<const char *>(bytes), amount, 0)
                            : recv(socket, reinterpret_cast<char *>(bytes), amount, 0);
#else
        int count = static_cast<int>(writing ? send(socket, bytes, static_cast<size_t>(amount), MSG_NOSIGNAL)
                                             : recv(socket, bytes, static_cast<size_t>(amount), 0));
#endif
        if (count < 0 && would_block(socket_error())) continue;
        if (count <= 0) throw std::runtime_error("Conexão IPC encerrada");
        bytes += count;
        size -= static_cast<size_t>(count);
    }
}

void connect_local(Connection &connection, uint16_t port, const std::string &token)
{
    connection.socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection.socket == invalid_socket) throw std::runtime_error("Socket IPC indisponível");
#ifdef _WIN32
    u_long nonblocking = 1;
    if (ioctlsocket(connection.socket, FIONBIO, &nonblocking)) throw std::runtime_error("Socket IPC inválido");
#else
    if (connection.socket >= FD_SETSIZE) throw std::runtime_error("Limite de sockets excedido");
    if (fcntl(connection.socket, F_SETFL, O_NONBLOCK) < 0) throw std::runtime_error("Socket IPC inválido");
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const auto deadline = Clock::now() + std::chrono::milliseconds(500);
    if (connect(connection.socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        if (!would_block(socket_error())) throw std::runtime_error("Worker IPC indisponível");
        ready(connection.socket, true, deadline);
        int error = 0;
#ifdef _WIN32
        int length = sizeof(error);
        if (getsockopt(connection.socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &length) != 0 || error)
#else
        socklen_t length = sizeof(error);
        if (getsockopt(connection.socket, SOL_SOCKET, SO_ERROR, &error, &length) != 0 || error)
#endif
            throw std::runtime_error("Worker IPC indisponível");
    }
    auto auth = std::vector<uint8_t>(token.begin(), token.end());
    transfer(connection.socket, auth.data(), auth.size(), true, deadline);
    std::array<uint8_t, 4> reply{};
    transfer(connection.socket, reply.data(), reply.size(), false, deadline);
    if (reply != std::array<uint8_t, 4>{'O', 'K', 'A', 'Y'}) throw std::runtime_error("Chave IPC recusada");
}

void put(std::array<uint8_t, 36> &header, size_t at, uint64_t value, size_t count)
{
    for (size_t i = 0; i < count; ++i) header[at + count - i - 1] = static_cast<uint8_t>(value >> (i * 8));
}
Frame exchange(Connection &connection, Frame frame)
{
    std::array<uint8_t, 36> header{'F', 'D', 'C', '1', 1, 1};
    put(header, 6, frame.format, 2); put(header, 8, frame.width, 4); put(header, 12, frame.height, 4);
    put(header, 16, frame.sequence, 8); put(header, 24, frame.timestamp, 8); put(header, 32, frame.pixels.size(), 4);
    const auto deadline = Clock::now() + std::chrono::milliseconds(500);
    transfer(connection.socket, header.data(), header.size(), true, deadline);
    transfer(connection.socket, frame.pixels.data(), frame.pixels.size(), true, deadline);
    std::array<uint8_t, 36> reply{};
    transfer(connection.socket, reply.data(), reply.size(), false, deadline);
    header[5] = 2;
    // The worker must preserve dimensions, format, timestamp, sequence and size.
    if (reply != header) throw std::runtime_error("Resposta IPC inválida");
    transfer(connection.socket, frame.pixels.data(), frame.pixels.size(), false, deadline);
    return frame;
}
} // namespace

Bridge::Bridge() : thread_(&Bridge::run, this) {}
Bridge::~Bridge()
{
    { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; }
    changed_.notify_all();
    thread_.join();
}
void Bridge::configure(bool enabled, uint16_t port, std::string token)
{
    bool valid = port && token.size() == 64 && std::all_of(token.begin(), token.end(), [](unsigned char c) {
        return std::isxdigit(c) != 0;
    });
    std::lock_guard<std::mutex> lock(mutex_);
    if (enabled_ == (enabled && valid) && port_ == port && token_ == token) return;
    enabled_ = enabled && valid;
    port_ = port; token_ = std::move(token); ++generation_;
    pending_.reset(); result_.reset(); round_trips_ = 0;
    status_ = enabled_ ? "Aguardando quadros e worker IPC" :
              (enabled ? "Preencha a porta e a chave exibidas pelo worker" : "IPC desativado");
    changed_.notify_all();
}
void Bridge::submit(Frame frame)
{
    if (frame.pixels.size() != payload_size(frame.format, frame.width, frame.height)) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_ || stopping_) return;
    frame.sequence = ++sequence_;
    frame.submitted = Clock::now();
    pending_ = std::move(frame);
    changed_.notify_one();
}
std::optional<Frame> Bridge::take(uint16_t format, uint32_t width, uint32_t height)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto frame = std::move(result_);
    result_.reset();
    if (!enabled_ || !frame || frame->format != format || frame->width != width || frame->height != height ||
        Clock::now() - frame->submitted > std::chrono::milliseconds(250)) return std::nullopt;
    return frame;
}
std::string Bridge::status() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}
void Bridge::run()
{
    try {
        SocketRuntime runtime;
        (void)runtime;
        Connection connection;
        uint64_t active_generation = 0;
        for (;;) {
            Frame frame;
            uint16_t port;
            std::string token;
            uint64_t generation;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                changed_.wait_for(lock, std::chrono::milliseconds(100), [&] {
                    return stopping_ || generation_ != active_generation || (enabled_ && pending_);
                });
                if (stopping_) return;
                if (generation_ != active_generation) {
                    connection.reset(); active_generation = generation_;
                }
                if (!enabled_ || !pending_) continue;
                frame = std::move(*pending_); pending_.reset();
                port = port_; token = token_; generation = generation_;
            }
            try {
                if (connection.socket == invalid_socket) connect_local(connection, port, token);
                auto reply = exchange(connection, std::move(frame));
                std::lock_guard<std::mutex> lock(mutex_);
                if (enabled_ && generation == generation_) {
                    result_ = std::move(reply);
                    status_ = "IPC conectado · quadros retornados: " + std::to_string(++round_trips_);
                }
            } catch (const std::exception &) {
                connection.reset();
                std::unique_lock<std::mutex> lock(mutex_);
                if (generation == generation_) {
                    result_.reset();
                    status_ = "Worker sem resposta ou chave inválida · mantendo vídeo original";
                }
                changed_.wait_for(lock, std::chrono::milliseconds(250), [&] {
                    return stopping_ || generation != generation_;
                });
            }
        }
    } catch (const std::exception &) {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = "Transporte IPC indisponível · mantendo vídeo original";
    }
}
} // namespace fdc
