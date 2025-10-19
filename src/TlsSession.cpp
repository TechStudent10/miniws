#include "TlsSession.hpp"

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

using namespace geode;

namespace ws {

std::string_view TlsError::message() const {
    static thread_local char buffer[WOLFSSL_MAX_ERROR_SZ];
    return wolfSSL_ERR_error_string(code, buffer);
}

inline void closeSocket(auto socket) {
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

TlsSession::~TlsSession() {
    if (ssl) {
        wolfSSL_free(ssl);
    }

    if (ctx) {
        wolfSSL_CTX_free(ctx);
    }

    if (fd != qsox::BaseSocket::InvalidSockFd) {
        closeSocket(fd);
    }
}

TlsSession::TlsSession(TlsSession&& other) {
    *this = std::move(other);
}

TlsSession& TlsSession::operator=(TlsSession&& other) {
    if (this != &other) {
        ctx = other.ctx;
        ssl = other.ssl;
        fd = other.fd;

        other.ctx = nullptr;
        other.ssl = nullptr;
        other.fd = qsox::BaseSocket::InvalidSockFd;
    }

    return *this;
}

TlsResult<TlsSession> TlsSession::create(qsox::TcpStream&& stream, bool insecure) {
    auto ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method());

    if (!ctx) {
        return Err(wolfSSL_ERR_get_error());
    }

    if (insecure) {
        wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_NONE, nullptr);
    } else {
        // TODO: proper cert handling
        if (wolfSSL_CTX_set_default_verify_paths(ctx) != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            return Err(wolfSSL_ERR_get_error());
        }

        wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_PEER, nullptr);
    }

    // Create session
    auto ssl = wolfSSL_new(ctx);
    if (!ssl) {
        wolfSSL_CTX_free(ctx);
        return Err(wolfSSL_ERR_get_error());
    }

    TlsSession session{ctx, ssl};

    auto fd = stream.releaseHandle();
    wolfSSL_set_fd(session.ssl, static_cast<int>(fd));

    // TODO: sni

    return Ok(std::move(session));
}

TlsResult<> TlsSession::handshake() {
    int res = wolfSSL_connect(ssl);
    if (res != WOLFSSL_SUCCESS) {
        return Err(wolfSSL_ERR_get_error());
    }

    return Ok();
}

TlsResult<size_t> TlsSession::send(const void* data, size_t size) {
    int res = wolfSSL_write(ssl, data, static_cast<int>(size));
    if (res < 0) {
        return Err(wolfSSL_ERR_get_error());
    }

    return Ok(static_cast<size_t>(res));
}

TlsResult<size_t> TlsSession::receive(void* buffer, size_t size) {
    int res = wolfSSL_read(ssl, buffer, static_cast<int>(size));
    if (res < 0) {
        return Err(wolfSSL_ERR_get_error());
    }

    return Ok(static_cast<size_t>(res));
}

TlsResult<> TlsSession::shutdown() {
    int res = wolfSSL_shutdown(ssl);
    if (res != WOLFSSL_SUCCESS) {
        return Err(wolfSSL_ERR_get_error());
    }

    return Ok();
}

}

static struct WolfsslInitGuard {
    WolfsslInitGuard() {
        wolfSSL_Init();
    }

    ~WolfsslInitGuard() {
        wolfSSL_Cleanup();
    }
} _g_wolfssl_init;
