#include <csignal>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <netinet/in.h>
#include <arpa/inet.h>

static std::filesystem::path spool;

int main(const int argc, const char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << "<port> <mail-spool-directory>" << std::endl;
        return 1;
    }
    const long port = strtol(argv[1], nullptr, 10);
    if (port < 1 || port > 65535) { std::cerr << "Invalid port number" << std::endl; return 1; }
    spool = std::filesystem::path(argv[2]);
    std::error_code ec;
    std::filesystem::create_directories(spool, ec);
    if (ec) { std::cerr << "Failed to create spool directory: " << ec.message() << std::endl; return 1; }
    signal(SIGPIPE, SIG_IGN); // We ignore a SIGPIPE, so that a disappeared client doesn't kill the server
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) { perror("socket"); return 1; }
    // Set the socket-option "Reuse Address", so that the server can reuse its port immediately after restarting & bypasses the default TCP "TIME_WAIT"
    int yes = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    // Set the sever address to 0.0.0.0:PORT, so it binds to all available interfaces on the defined port
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(static_cast<uint16_t>(port));
    // Bind the socket to the address...
    if (bind(server_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) < 0) { perror("bind"); return 1; }
    // ... and start listening
    if (listen(server_socket, 5) < 0) { perror("listen"); return 1; }
    std::cout << "Listening on port " << port << std::endl;

    while (true) {
        sockaddr_in client_address{};
        socklen_t client_address_length = sizeof(client_address);
        int fd = accept(server_socket, reinterpret_cast<sockaddr*>(&client_address), &client_address_length);
        if (fd < 0) {
            if (errno == EINTR) continue; // Stray system interrupts shouldn't kill the server, so we ignore them
            perror("accept");
            break;
        }
        std::cout << "Client connected:" << inet_ntoa(client_address.sin_addr) << std::endl;
        // TODO actual request handling
        close(fd);
        std::cout << "Client disconnected" << std::endl;
    }
    close(server_socket);
    return 0;
}
