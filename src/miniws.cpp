#include <algorithm>

#include <map>
#include <random>

#include <qsox/TcpStream.hpp>
#include <qsox/Resolver.hpp>
#include <miniws.hpp>

// #include <cpr/cpr.h>
#include <base64.hpp>
#include <fmt/base.h>
#include <fmt/format.h>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

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
        std::string url = address.url;
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

    void Client::open(ServerAddress address) {
        if (isConnected()) {
            error("already connected!");
            return;
        }

        wolfSSL_Init();
        WOLFSSL_CTX* ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
        wolfSSL_CTX_load_verify_locations(ctx, "ca-cert.pem", NULL);

        ssl = wolfSSL_new(ctx);

        // creating the socket
        this->address = address;

        std::string url = address.url;
        uint16_t port = address.port;
        std::string path = address.path;

        auto resolveRes = qsox::resolver::resolve(url);
        if (resolveRes.isErr()) {
            error(fmt::format("error resolving address: {}", resolveRes.unwrapErr().message()));
            return;
        }

        info(fmt::format("resolved address: {}", resolveRes.unwrap().toString()));

        auto streamRes = TcpStream::connect({resolveRes.unwrap(), port});
        if (streamRes.isErr()) {
            error(fmt::format("error connecting to tcpstream: {}", streamRes.unwrapErr().message()));
            return;
        }

        stream = std::make_shared<qsox::TcpStream>(std::move(streamRes).unwrap());

        watchThread = std::thread([this]() {
            this->watch();
        });
        watchThread.detach();
    }

    void Client::watch() {
        std::string request = createHandshakeRequest(address);

        CHECK_UNWRAP(
            stream->send(request.c_str(), request.size()),
            "unable to send handshake request: {}", res.unwrapErr().message()
        )

        char buffer[4096];
        CHECK_UNWRAP(
            stream->receive(buffer, sizeof(buffer) - 1),
            "unable to receive handshake response: {}", res.unwrapErr().message()
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
                "unable to receieve message: {}", res.unwrapErr().message()
            )

            bool fin = ws_buffer[0] & 0x80;
            uint8_t opcode = ws_buffer[0] & 0x0F;
            bool masked = ws_buffer[1] & 0x80;
            uint64_t len = ws_buffer[1] & 0x7F;

            if (len == 126) {
                uint8_t ext[2];
                CHECK_UNWRAP(
                    stream->receiveExact(ext, 2),
                    "unable to get length: {}", res.unwrapErr().message()
                )
                len = (ext[0] << 8) | ext[1];
            } else if (len == 127) {
                uint8_t ext[8];
                CHECK_UNWRAP(
                    stream->receiveExact(ext, 8),
                    "unable to get length: {}", res.unwrapErr().message()
                )
                len = 0;
                for (int i = 0; i < 8; ++i)
                    len = (len << 8) | ext[i];
            }

            uint8_t recv_masking_key[4];
            if (masked) {
                CHECK_UNWRAP(
                    stream->receiveExact(recv_masking_key, 4),
                    "unable to get masking key: {}", res.unwrapErr().message()
                )
            }

            std::vector<uint8_t> payload(len);
            CHECK_UNWRAP(
                stream->receiveExact(payload.data(), len),
                "unable to receieve payload: {}", res.unwrapErr().message()
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
            "unable to send message frame: {}", res.unwrapErr().message()
        )
    }

    void Client::close() {
        if (stream) {
            CHECK_UNWRAP(
                stream->shutdown(qsox::ShutdownMode::Both),
                "unable to shutdown stream: {}", res.unwrapErr().message()
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
