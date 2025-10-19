#pragma once

#include <Geode/Result.hpp>
#include <qsox/BaseSocket.hpp>
#include <qsox/TcpStream.hpp>

struct WOLFSSL_CTX;
struct WOLFSSL;

namespace ws {

struct TlsError {
    unsigned long code;
    constexpr inline TlsError(unsigned long code) : code(code) {}
    constexpr inline TlsError(const TlsError& other) = default;

    std::string_view message() const;
};

template <typename T = void>
using TlsResult = geode::Result<T, TlsError>;

class TlsSession {
public:
    WOLFSSL_CTX* ctx = nullptr;
    WOLFSSL* ssl = nullptr;
    qsox::SockFd fd = qsox::BaseSocket::InvalidSockFd;

    static TlsResult<TlsSession> create(qsox::TcpStream&& stream, bool insecure);
    ~TlsSession();

    TlsSession(const TlsSession&) = delete;
    TlsSession& operator=(const TlsSession&) = delete;
    TlsSession(TlsSession&&);
    TlsSession& operator=(TlsSession&&);

    TlsResult<> handshake();

    TlsResult<size_t> send(const void* data, size_t size);
    TlsResult<size_t> receive(void* buffer, size_t size);
    TlsResult<> shutdown();

private:
    TlsSession(WOLFSSL_CTX* ctx, WOLFSSL* ssl) : ctx(ctx), ssl(ssl) {}
};

}
