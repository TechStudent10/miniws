#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <optional>
#include "BaseTransport.hpp"

// #include <qsox/TcpStream.hpp>

// using namespace qsox;

namespace qsox {
    class TcpStream;
}

namespace ws {
    struct ServerAddress {
        std::string host;
        int port;
        bool secure;
        std::string path = "/";
    };

    enum class LogSeverity {
        Info,
        Debug,
        Error
    };

    class Client {
    private:
        std::shared_ptr<BaseTransport> stream;
        bool connected = false;
        std::thread watchThread;
        ServerAddress address;

        std::string createHandshakeRequest(ServerAddress address);
        std::vector<uint8_t> createMessageFrame(std::string message);

        // queue to send messages upon connection
        std::vector<std::string> queue;

        std::function<void(std::string)> msgCallback;
        std::function<void(LogSeverity, std::string)> logCallback;

        void info(std::string message) {
            logCallback(LogSeverity::Info, message);
        }
        void debug(std::string message) {
            logCallback(LogSeverity::Debug, message);
        }
        void error(std::string message) {
            logCallback(LogSeverity::Error, message);
        }

        void watch();

    public:
        Client();
        ~Client() noexcept {
            close();
        }


        bool isConnected() {
            return connected;
        }

        geode::Result<> open(ServerAddress address);
        geode::Result<> open(std::string_view url);
        void close();

        void send(std::string data);

        void onMessage(std::function<void(std::string)> callback) {
            msgCallback = callback;
        }

        void onLog(std::function<void(LogSeverity, std::string)> callback) {
            logCallback = callback;
        }

        static std::string severityToString(LogSeverity);
    };
}