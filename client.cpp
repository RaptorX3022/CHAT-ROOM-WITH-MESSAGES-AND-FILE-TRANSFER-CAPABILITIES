#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <iterator>
#include <sstream>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/socket.h>
#pragma comment(lib, "Ws2_32.lib")

#define PORT "8080"
#define SERVER_ADDRESS "127.0.0.1"

void receive_messages(SOCKET sock) {
    char buffer[1024];
    while (true) {
        int valread = recv(sock, buffer, sizeof(buffer), 0);
        if (valread <= 0) {
            break;
        }
        buffer[valread] = '\0';
        std::cout << "Server: " << buffer << std::endl;
    }
}

void send_file(SOCKET sock, const std::string& file_path) {
    // Open the file
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return;
    }

    // Determine file size
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Send file size
    send(sock, reinterpret_cast<const char*>(&file_size), sizeof(file_size), 0);

    // Read and send file contents in chunks
    const int buffer_size = 1024;
    char buffer[buffer_size];
    while (!file.eof()) {
        file.read(buffer, buffer_size);
        send(sock, buffer, static_cast<int>(file.gcount()), 0);
    }

    file.close();
}

void receive_file(SOCKET sock, const std::string& file_path) {
    // Open the file
    std::ofstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << file_path << std::endl;
        return;
    }

    // Receive file size
    std::streamsize file_size;
    recv(sock, reinterpret_cast<char*>(&file_size), sizeof(file_size), 0);

    // Receive and write file contents in chunks
    const int buffer_size = 1024;
    char buffer[buffer_size];
    std::streamsize received_size = 0;
    while (received_size < file_size) {
        int bytes_received = recv(sock, buffer, buffer_size, 0);
        if (bytes_received <= 0) {
            std::cerr << "Error receiving file" << std::endl;
            break;
        }
        file.write(buffer, bytes_received);
        received_size += bytes_received;
    }

    file.close();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <client_name>" << std::endl;
        return 1;
    }

    std::string client_name = argv[1];

    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    struct addrinfo hints = {0}, *addrinfo_result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    result = getaddrinfo(SERVER_ADDRESS, PORT, &hints, &addrinfo_result);
    if (result != 0) {
        std::cerr << "getaddrinfo failed: " << result << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET sock = socket(addrinfo_result->ai_family, addrinfo_result->ai_socktype, addrinfo_result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(addrinfo_result);
        WSACleanup();
        return 1;
    }

    result = connect(sock, addrinfo_result->ai_addr, (int)addrinfo_result->ai_addrlen);
    if (result == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        freeaddrinfo(addrinfo_result);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(addrinfo_result);

    std::thread(receive_messages, sock).detach();

    while (true) {
        std::string input;
        std::getline(std::cin, input);

        if (input == "exit") {
            break;
        } else if (input.find("sendfile") == 0) {
            // Extract file path from input
            std::istringstream iss(input);
            std::vector<std::string> tokens(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());
            if (tokens.size() < 2) {
                std::cerr << "Usage: sendfile <file_path>" << std::endl;
                continue;
            }
            std::string file_path = tokens[1];

            // Send file
            send_file(sock, file_path);
        } else if (input.find("downloadfile") == 0) {
            // Extract file path from input
            std::istringstream iss(input);
            std::vector<std::string> tokens(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());
            if (tokens.size() < 2) {
                std::cerr << "Usage: downloadfile <file_path_on_server>" << std::endl;
                continue;
            }
            std::string file_path_on_server = tokens[1];

            // Receive file
            receive_file(sock, file_path_on_server);
        } else {
            // Send message
            std::string message = client_name + ": " + input;
            send(sock, message.c_str(), message.length(), 0);
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
