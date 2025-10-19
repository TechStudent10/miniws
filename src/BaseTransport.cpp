#include <BaseTransport.hpp>
#include <stdint.h>

using namespace geode;

namespace ws {

Result<> BaseTransport::receiveExact(void* buffer, size_t size) {
    size_t totalReceived = 0;
    uint8_t* bufPtr = static_cast<uint8_t*>(buffer);

    while (totalReceived < size) {
        GEODE_UNWRAP_INTO(size_t received, this->receive(bufPtr + totalReceived, size - totalReceived));

        if (received == 0) {
            return Err("Connection closed before receiving all data");
        }

        totalReceived += received;
    }

    return Ok();
}

Result<size_t> BaseTransport::sendAll(const void* data, size_t size) {
    size_t totalSent = 0;
    const uint8_t* dataPtr = static_cast<const uint8_t*>(data);

    while (totalSent < size) {
        GEODE_UNWRAP_INTO(size_t sent, this->send(dataPtr + totalSent, size - totalSent));

        if (sent == 0) {
            return Err("Connection closed before sending all data");
        }

        totalSent += sent;
    }

    return Ok(totalSent);
}

}