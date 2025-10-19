#pragma once

#include <BaseTransport.hpp>
#include <qsox/TcpStream.hpp>
#include <memory>

namespace ws {

class TcpTransport : public BaseTransport {
public:
    static geode::Result<std::shared_ptr<BaseTransport>> connect(const qsox::SocketAddress& address);

    geode::Result<size_t> send(const void* data, size_t size) override;
    geode::Result<size_t> receive(void* buffer, size_t size) override;
    geode::Result<> shutdown() override;

    TcpTransport(qsox::TcpStream&& stream) : stream(std::move(stream)) {}

private:
    qsox::TcpStream stream;
};

}