# miniws

minimal WebSocket client library written in C++

## features

basic WebSocket connection, sending and receiving text (TODO: binary)

## usage

see [here](/test/main.cpp)

```cpp
#include <iostream>
#include <miniws.hpp>

using namespace ws;

int main() {
    auto client = new Client();
    client->open({
        .url = "localhost",
        .port = 8080
    });
    client->onMessage([](std::string message) {
        std::cout << "[Server] " + message << std::endl;
    });
    client->send("yo! whats up");

    std::cout << "say something to the server!" << std::endl;
    while (true) {
        std::string input;
        std::cin >> input;

        client->send(input);
    }

    client->close();

    return 0;
}
```

## credits

this project would not be possible without:

- [**qsox**](https://github.com/dankmeme01/qsox) - all the low-level socket stuff, plus the developer helped me out in making this library :D
- [**fmt**](https://github.com/fmtlib/fmt) - formatting/logging library
- [**wolfSSL**](https://github.com/wolfSSL/wolfssl) - TLS/SSL support
