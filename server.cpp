#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT "8080"

std::vector<SOCKET> clients;

void handle_client(SOCKET client_socket) {
    char buffer[1024];
    while (true) {
        int valread = recv(client_socket, buffer, sizeof(buffer), 0);
        if (valread <= 0) {
            break;
        }
        buffer[valread] = '\0';

        // Check if the message is a file transfer request
        if (std::string(buffer).find("sendfile") == 0) {
            // Extract file path from message
            std::string file_path = std::string(buffer).substr(8); // "sendfile " has 8 characters

            // Receive file from client
            std::ofstream file(file_path, std::ios::binary);
            if (!file.is_open()) {
                std::cerr << "Failed to create file: " << file_path << std::endl;
                continue;
            }

            // Receive file size
            std::streamsize file_size;
            recv(client_socket, reinterpret_cast<char*>(&file_size), sizeof(file_size), 0);

            // Receive and write file contents in chunks
            const int buffer_size = 1024;
            char buffer[buffer_size];
            std::streamsize received_size = 0;
            while (received_size < file_size) {
                int bytes_received = recv(client_socket, buffer, buffer_size, 0);
                if (bytes_received <= 0) {
                    std::cerr << "Error receiving file" << std::endl;
                    break;
                }
                file.write(buffer, bytes_received);
                received_size += bytes_received;
            }

            file.close();
            std::cout << "Received file: " << file_path << std::endl;
        } else if (std::string(buffer).find("downloadfile") == 0) {
            // Extract file path from message
            std::string file_path_on_server = std::string(buffer).substr(13); // "downloadfile " has 13 characters

            // Send file to client
            std::ifstream file(file_path_on_server, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                std::cerr << "Failed to open file: " << file_path_on_server << std::endl;
                continue;
            }

            // Determine file size
            std::streamsize file_size = file.tellg();
            file.seekg(0, std::ios::beg);

            // Send file size
            send(client_socket, reinterpret_cast<const char*>(&file_size), sizeof(file_size), 0);

            // Read and send file contents in chunks
            const int buffer_size = 1024;
            char buffer[buffer_size];
            while (!file.eof()) {
                file.read(buffer, buffer_size);
                send(client_socket, buffer, static_cast<int>(file.gcount()), 0);
            }

            file.close();
            std::cout << "Sent file: " << file_path_on_server << std::endl;
        } else {
            // Broadcast message to all clients
            std::cout << "Client: " << buffer << std::endl;
            for (SOCKET client : clients) {
                if (client != client_socket) {
                    send(client, buffer, valread, 0);
                }
            }
        }
    }

    // Client disconnected
    closesocket(client_socket);
}

int main() {
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
    hints.ai_flags = AI_PASSIVE;

    result = getaddrinfo(nullptr, PORT, &hints, &addrinfo_result);
    if (result != 0) {
        std::cerr << "getaddrinfo failed: " << result << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET server_socket = socket(addrinfo_result->ai_family, addrinfo_result->ai_socktype, addrinfo_result->ai_protocol);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(addrinfo_result);
        WSACleanup();
        return 1;
    }

    result = bind(server_socket, addrinfo_result->ai_addr, (int)addrinfo_result->ai_addrlen);
    if (result == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(addrinfo_result);
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(addrinfo_result);

    result = listen(server_socket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server started, waiting for connections..." << std::endl;

    while (true) {
        SOCKET client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            closesocket(server_socket);
            WSACleanup();
            return 1;
        }
        clients.push_back(client_socket);
        std::thread(handle_client, client_socket).detach();
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}
