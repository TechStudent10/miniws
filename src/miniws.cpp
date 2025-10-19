#include <algorithm>

#include <map>
#include <random>
#include <charconv>

#include <qsox/TcpStream.hpp>
#include <qsox/Resolver.hpp>
#include <miniws.hpp>
#include "TlsTransport.hpp"
#include "TcpTransport.hpp"

// #include <cpr/cpr.h>
#include <base64.hpp>
#include <fmt/base.h>
#include <fmt/format.h>

using namespace qsox;

static void fillRandom(uint8_t* dest, size_t len) {
    static std::random_device rd;
    static std::mt19937_64 generator(rd());

    while (len > 8) {
        uint64_t value = generator();
        std::memcpy(dest, &value, sizeof(value));
        dest += sizeof(value);
        len -= sizeof(value);
    }

    // less than 8 bytes left
    while (len > 0) {
        uint8_t value = static_cast<uint8_t>(generator() % 256);
        *dest++ = value;
        --len;
    }
}

#define CHECK_UNWRAP(statement, ...) if (auto res = statement; res.isErr()) { error(fmt::format(__VA_ARGS__)); return; }

namespace ws {
    Client::Client() {
        // set default logging function
        onLog([](LogSeverity severity, std::string message) {
            fmt::println("[{}] {}", severityToString(severity), message);
        });
    }

    std::string Client::createHandshakeRequest(ServerAddress address) {
        std::string url = address.host;
        int port = address.port;
        std::string path = address.path;


        std::array<uint8_t, 16> random_bytes;
        fillRandom(random_bytes.data(), random_bytes.size());

        std::string_view bytes(reinterpret_cast<char*>(random_bytes.data()), random_bytes.size());

        std::string request =
            "GET " + path + " HTTP/1.1\r\n"
            "Host: " + url + ":" + std::to_string(port) + "\r\n"
            "Origin: http://" + url + "\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: " + base64_encode(bytes) + "\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n";

        return request;
    }

    std::vector<uint8_t> Client::createMessageFrame(std::string message) {
        std::vector<uint8_t> frame;
        frame.push_back(0x80 | 0x1); // FIN + opcode
        frame.push_back(0x80 | message.size()); // mask + payload size

        // generated with chatgpt bc im dumb
        // Generate masking key
        uint8_t masking_key[4];
        for (int i = 0; i < 4; ++i)
            masking_key[i] = rand() % 256;

        frame.insert(frame.end(), masking_key, masking_key + 4);

        // Mask the payload
        for (size_t i = 0; i < message.size(); ++i) {
            frame.push_back(message[i] ^ masking_key[i % 4]);
        }

        return frame;
    }

    Result<> Client::open(std::string_view url) {
        ServerAddress addr{};

        if (url.starts_with("ws://")) {
            addr.secure = false;
            url.remove_prefix(5);
        } else if (url.starts_with("wss://")) {
            addr.secure = true;
            url.remove_prefix(6);
        } else {
            return Err("invalid url scheme");
        }

        size_t pathPos = url.find('/');
        if (pathPos != std::string_view::npos) {
            addr.path = std::string(url.substr(pathPos));
            url = url.substr(0, pathPos);
        }

        size_t portPos = url.find(':');
        if (portPos != std::string_view::npos) {
            addr.host = std::string(url.substr(0, portPos));
            auto [ptr, ec] = std::from_chars(url.data() + portPos + 1, url.data() + url.size(), addr.port);
            if (ec != std::errc()) {
                return Err("invalid port");
            }
        } else {
            addr.host = std::string(url);
            addr.port = addr.secure ? 443 : 80;
        }

        return this->open(std::move(addr));
    }

    Result<> Client::open(ServerAddress address) {
        if (this->isConnected()) {
            return Err("already connected!");
        }

        // bool secure = address.url.starts_with("wss://");
        bool secure = true;

        this->address = address;

        std::string url = address.host;
        uint16_t port = address.port;
        std::string path = address.path;

        auto resolveRes = qsox::resolver::resolve(url);
        if (resolveRes.isErr()) {
            return Err(fmt::format("failed to resolve address: {}", resolveRes.unwrapErr().message()));
        }

        info(fmt::format("resolved address: {}", resolveRes.unwrap().toString()));

        if (secure) {
            GEODE_UNWRAP_INTO(stream, TlsTransport::connect({resolveRes.unwrap(), port}));
        } else {
            GEODE_UNWRAP_INTO(stream, TcpTransport::connect({resolveRes.unwrap(), port}));
        }

        watchThread = std::thread([this]() {
            this->watch();
        });
        watchThread.detach();

        return Ok();
    }

    void Client::watch() {
        std::string request = createHandshakeRequest(address);

        CHECK_UNWRAP(
            stream->send(request.c_str(), request.size()),
            "unable to send handshake request: {}", res.unwrapErr()
        )

        char buffer[4096];
        CHECK_UNWRAP(
            stream->receive(buffer, sizeof(buffer) - 1),
            "unable to receive handshake response: {}", res.unwrapErr()
        )

        if (std::string(buffer).find("HTTP/1.1 101") == std::string::npos) {
            error("handshake did NOT succeed...");
            return;
        }

        info("handshake complete; watching for messages...");
        connected = true;

        std::thread([this]() {
            for (auto msg : queue) {
                send(msg);
            }
        }).detach();

        // start thread with message frames
        while (isConnected()) {
            char ws_buffer[2];
            CHECK_UNWRAP(
                stream->receiveExact(ws_buffer, 2),
                "unable to receieve message: {}", res.unwrapErr()
            )

            bool fin = ws_buffer[0] & 0x80;
            uint8_t opcode = ws_buffer[0] & 0x0F;
            bool masked = ws_buffer[1] & 0x80;
            uint64_t len = ws_buffer[1] & 0x7F;

            if (len == 126) {
                uint8_t ext[2];
                CHECK_UNWRAP(
                    stream->receiveExact(ext, 2),
                    "unable to get length: {}", res.unwrapErr()
                )
                len = (ext[0] << 8) | ext[1];
            } else if (len == 127) {
                uint8_t ext[8];
                CHECK_UNWRAP(
                    stream->receiveExact(ext, 8),
                    "unable to get length: {}", res.unwrapErr()
                )
                len = 0;
                for (int i = 0; i < 8; ++i)
                    len = (len << 8) | ext[i];
            }

            uint8_t recv_masking_key[4];
            if (masked) {
                CHECK_UNWRAP(
                    stream->receiveExact(recv_masking_key, 4),
                    "unable to get masking key: {}", res.unwrapErr()
                )
            }

            std::vector<uint8_t> payload(len);
            CHECK_UNWRAP(
                stream->receiveExact(payload.data(), len),
                "unable to receieve payload: {}", res.unwrapErr()
            )

            if (masked) {
                for (size_t i = 0; i < len; ++i)
                    payload[i] ^= recv_masking_key[i % 4];
            }

            msgCallback(std::string(payload.begin(), payload.end()));
        }

        close();
    }

    void Client::send(std::string data) {
        if (!isConnected()) {
            info("adding to queue");
            queue.push_back(data);
            return;
        }

        std::vector<uint8_t> frame = createMessageFrame(data);

        CHECK_UNWRAP(
            stream->send(frame.data(), frame.size()),
            "unable to send message frame: {}", res.unwrapErr()
        )
    }

    void Client::close() {
        if (stream) {
            CHECK_UNWRAP(
                stream->shutdown(),
                "unable to shutdown stream: {}", res.unwrapErr()
            )
        }
    }

    std::string Client::severityToString(LogSeverity severity) {
        std::map<LogSeverity, std::string> severityMap = {
            { LogSeverity::Debug, "Debug" },
            { LogSeverity::Error, "Error" },
            { LogSeverity::Info, "Info" }
        };
        return severityMap[severity];
    }
}
