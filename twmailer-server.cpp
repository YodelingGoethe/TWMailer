#include "common.h"
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <ranges>
#include <vector>
#include <netinet/in.h>
#include <arpa/inet.h>

static std::filesystem::path spool;

static bool validate_username(const std::string &username) {
    if (username.empty() || username.size() > 8) return false;
    return std::all_of(username.begin(), username.end(), [](const char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z');
    });
}

static std::vector<std::filesystem::path> get_messages(const std::string &username) {
    std::vector<std::pair<long, std::filesystem::path>> unsorted_messages;
    for (std::error_code ec; const auto &entry : std::filesystem::directory_iterator(spool/username, ec)) {
        if (entry.path().extension() == ".msg") {
            unsorted_messages.emplace_back(strtol(entry.path().stem().string().c_str(), nullptr, 10), entry.path());
        }
    }
    std::sort(unsorted_messages.begin(), unsorted_messages.end());
    std::vector<std::filesystem::path> sorted_messages;
    for (const auto &message: unsorted_messages | std::views::values) sorted_messages.push_back(message);
    return sorted_messages;
}

static bool find_message(const std::string &username, const std::string &number, std::filesystem::path &out) {
    if (!validate_username(username) || number.empty() || !std::all_of(number.begin(), number.end(), ::isdigit))
        return false;
    const auto messages = get_messages(username);
    const size_t n = stoul(number);
    if (n < 1 || n > messages.size()) return false;
    out = messages[n-1]; // n - 1, since the message numbers are 1-based
    return true;
}

static bool handleSend(LineReader &in, const int fd) {
    std::string sender, receiver, subject, line, body;
    if (!in.readLine(sender) || in.readLine(receiver) || in.readLine(subject)) return false;
    while (true) {
        if (!in.readLine(line)) return false;
        if (line == ".") break;
        body += line + "\n";
    }
    if (!validate_username(sender) || !validate_username(receiver) || subject.size() > 80) {
        std::cerr << "SEND rejected: Invalid sender/receiver/subject" << std::endl;
        return sendAll(fd, "ERR\n");
    }
    std::error_code ec;
    std::filesystem::create_directories(spool/receiver , ec);
    const auto messages = get_messages(receiver);
    long next = messages.empty() ? 1 : strtol(messages.back().stem().string().c_str(), nullptr, 10) + 1;
    std::ofstream out(spool/receiver/(std::to_string(next)+".msg"));
    out << sender << "\n" << receiver << "\n" << subject << "\n" << body << std::flush;
    if (!out) {
        std::cerr << "Failed to write to message file" << std::endl;
        return sendAll(fd, "ERR\n");
    }
    return sendAll(fd, "OK\n");
}

static bool handleList(LineReader &in, const int fd) {
    std::string username;
    if (!in.readLine(username)) return false;
    if (!validate_username(username)) return sendAll(fd, "0\n");
    std::ostringstream out;
    auto messages = get_messages(username);
    out << messages.size() << "\n";
    for (const auto &message: messages) {
        std::ifstream file(message);
        std::string sender, receiver, subject;
        std::getline(file, sender);
        std::getline(file, receiver);
        std::getline(file, subject);
        out << subject << "\n";
    }
    return sendAll(fd, out.str());
}

static bool handleRead(LineReader &in, const int fd) {
    std::string username, message_number;
    if (!in.readLine(username) || !in.readLine(message_number)) return false;
    std::filesystem::path path;
    if (!find_message(username, message_number, path)) return sendAll(fd, "ERR\n");
    std::ifstream file(path);
    std::stringstream ss;
    ss << file.rdbuf();
    return sendAll(fd, "OK\n" + ss.str() + ".\n");
}

static bool handleDel(LineReader &in, const int fd) {
    std::string username, message_number;
    if (!in.readLine(username) || !in.readLine(message_number)) return false;
    if (std::filesystem::path path; !find_message(username, message_number, path) || std::filesystem::remove(path))
        return sendAll(fd, "ERR\n");
    return sendAll(fd, "OK\n");
}

static void serve_client(const int fd) {
    LineReader in(fd);
    std::string cmd;
    while (in.readLine(cmd)) {
        bool ok;
        if (cmd == "SEND") ok = handleSend(in, fd);
        else if (cmd == "LIST") ok = handleList(in, fd);
        else if (cmd == "READ") ok = handleRead(in, fd);
        else if (cmd == "DEL") ok = handleDel(in, fd);
        else if (cmd == "QUIT") { std::cout << "QUIT" << std::endl; return; }
        else { std::cerr << "Unknown command: " << cmd << std::endl; ok = sendAll(fd, "ERR\n"); }
        if (!ok) { std::cerr << "Failed to send" << std::endl; return; }
    }
}

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
        serve_client(fd);
        close(fd);
        std::cout << "Client disconnected" << std::endl;
    }
    close(server_socket);
    return 0;
}
