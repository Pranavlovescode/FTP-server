// server.cpp (Windows)
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h> // for _getcwd
#define getcwd _getcwd
#pragma comment(lib, "Ws2_32.lib")
typedef int socklen_t;
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#define closesocket close
#define SOCKET int
#define INVALID_SOCKET -1
#endif
#include <filesystem>

#define PORT 2121
#define BUFFER_SIZE 1024
namespace fs = std::filesystem;

void sendFileList(SOCKET client_socket)
{
    std::string files;

#ifdef _WIN32
    WIN32_FIND_DATA fileData;
    HANDLE hFind = FindFirstFile("*", &fileData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            files += fileData.cFileName;
            files += "\n";
        } while (FindNextFile(hFind, &fileData));
        FindClose(hFind);
    }
#else
    DIR *dir;
    struct dirent *entry;
    if ((dir = opendir(".")) != nullptr)
    {
        while ((entry = readdir(dir)) != nullptr)
        {
            files += entry->d_name;
            files += "\n";
        }
        closedir(dir);
    }
#endif

    send(client_socket, files.c_str(), files.size(), 0);
}

void handleClient(SOCKET client_socket)
{
    char buffer[BUFFER_SIZE];

    while (true)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0)
            break;

        std::string command(buffer);

        if (command == "LIST")
        {
            sendFileList(client_socket);
        }
        else if (command.rfind("GET ", 0) == 0)
        {
            std::string filename = command.substr(4);
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file.is_open())
            {
                std::string msg = "ERROR: File not found\n";
                send(client_socket, msg.c_str(), msg.size(), 0);
                continue;
            }

            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            // Send size first
            std::string size_str = std::to_string(size);
            send(client_socket, size_str.c_str(), size_str.size(), 0);
            Sleep(100); // Optional: small delay so client reads size first

            char buffer[1024];
            while (!file.eof())
            {
                file.read(buffer, sizeof(buffer));
                send(client_socket, buffer, file.gcount(), 0);
            }

            file.close();
        }

        else if (command.rfind("PUT ", 0) == 0)
        {
            std::string filename = command.substr(4);

            // Check if file already exists
            if (fs::exists(filename))
            {
                std::string msg = "ERROR: File already exists on server\n";
                send(client_socket, msg.c_str(), msg.size(), 0);
                continue;
            }

            // Send OK response to signal client to send file size and data
            std::string ok = "OK";
            send(client_socket, ok.c_str(), ok.size(), 0);

            // Receive file size
            char size_buf[64];
            int size_bytes = recv(client_socket, size_buf, sizeof(size_buf), 0);
            size_buf[size_bytes] = '\0';
            int file_size = std::stoi(size_buf);

            std::ofstream file(filename, std::ios::binary);
            if (!file.is_open())
            {
                std::string msg = "ERROR: Cannot write file\n";
                send(client_socket, msg.c_str(), msg.size(), 0);
                continue;
            }

            char buffer[1024];
            int received = 0;

            while (received < file_size)
            {
                int bytes = recv(client_socket, buffer, sizeof(buffer), 0);
                if (bytes <= 0)
                    break;
                file.write(buffer, bytes);
                received += bytes;
            }

            file.close();

            std::string success = "Upload complete\n";
            send(client_socket, success.c_str(), success.size(), 0);
        }

        else if (command == "QUIT")
        {
            break;
        }
        else
        {
            std::string msg = "Invalid command\n";
            send(client_socket, msg.c_str(), msg.size(), 0);
        }
    }

    closesocket(client_socket);
}

int main()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }
#endif

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed.\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr *)&address, sizeof(address)) < 0)
    {
        std::cerr << "Bind failed.\n";
        closesocket(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    listen(server_fd, 3);
    std::cout << "Server started on port " << PORT << "...\n";

    while (true)
    {
        SOCKET client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET)
        {
            std::cerr << "Failed to accept connection.\n";
            break;
        }
        std::cout << "Client connected.\n";
        std::thread(handleClient, client_socket).detach();
    }

    closesocket(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
