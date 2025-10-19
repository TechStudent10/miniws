#pragma once

#include <Geode/Result.hpp>

namespace ws {

class BaseTransport {
public:
    virtual ~BaseTransport() = default;

    virtual geode::Result<size_t> send(const void* data, size_t size) = 0;
    virtual geode::Result<size_t> receive(void* buffer, size_t size) = 0;
    virtual geode::Result<> shutdown() = 0;

    virtual geode::Result<> receiveExact(void* buffer, size_t size);
    virtual geode::Result<size_t> sendAll(const void* data, size_t size);
};

}