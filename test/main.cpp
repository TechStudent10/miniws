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
        if (input == "exit") {
            break;
        }

        client->send(input);
    }

    client->close();

    return 0;
}
