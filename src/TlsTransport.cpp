#include "TlsTransport.hpp"

using namespace geode;

namespace ws {

template <typename T>
auto mapResult(T&& res) {
    return std::forward<T>(res).mapErr([](const auto& err) { return std::string{err.message()}; });
}

Result<std::shared_ptr<BaseTransport>> TlsTransport::connect(const qsox::SocketAddress& address) {
    GEODE_UNWRAP_INTO(auto stream, mapResult(qsox::TcpStream::connect(address)));
    GEODE_UNWRAP_INTO(auto session, mapResult(TlsSession::create(std::move(stream), true)));
    GEODE_UNWRAP(mapResult(session.handshake()));

    return Ok(std::make_shared<TlsTransport>(std::move(session)));
}

Result<size_t> TlsTransport::send(const void* data, size_t size) {
    return mapResult(session.send(data, size));
}

Result<size_t> TlsTransport::receive(void* buffer, size_t size) {
    return mapResult(session.receive(buffer, size));
}

Result<> TlsTransport::shutdown() {
    return mapResult(session.shutdown());
}

}