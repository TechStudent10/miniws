#include "TcpTransport.hpp"

using namespace geode;

namespace ws {

template <typename T>
auto mapResult(T&& res) {
    return std::forward<T>(res).mapErr([](const auto& err) { return std::string{err.message()}; });
}

Result<std::shared_ptr<BaseTransport>> TcpTransport::connect(const qsox::SocketAddress& address) {
    GEODE_UNWRAP_INTO(auto stream, mapResult(qsox::TcpStream::connect(address)));
    return Ok(std::make_shared<TcpTransport>(std::move(stream)));
}

Result<size_t> TcpTransport::send(const void* data, size_t size) {
    return mapResult(stream.send(data, size));
}

Result<size_t> TcpTransport::receive(void* buffer, size_t size) {
    return mapResult(stream.receive(buffer, size));
}

Result<> TcpTransport::shutdown() {
    return mapResult(stream.shutdown(qsox::ShutdownMode::Both));
}

}