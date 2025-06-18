// client.cpp (Windows)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#define PORT 2121

void sendCommand(SOCKET sock, const std::string &cmd)
{
    send(sock, cmd.c_str(), cmd.size(), 0);
}

void receiveAndPrint(SOCKET sock)
{
    char buffer[1024];
    int bytes = recv(sock, buffer, sizeof(buffer), 0);
    buffer[bytes] = '\0';
    std::cout << buffer;
}

void downloadFile(SOCKET sock, const std::string &filename)
{
    char size_buf[64] = {0};
    int size_bytes = recv(sock, size_buf, sizeof(size_buf) - 1, 0);  // leave space for null terminator

    if (size_bytes <= 0) {
        std::cerr << "Failed to receive file size from server.\n";
        return;
    }

    size_buf[size_bytes] = '\0';
    std::string size_str(size_buf);

    int file_size = 0;
    try {
        file_size = std::stoi(size_str);
    } catch (const std::invalid_argument &e) {
        std::cerr << "Invalid file size received from server: '" << size_str << "'\n";
        return;
    } catch (const std::out_of_range &e) {
        std::cerr << "File size out of range: '" << size_str << "'\n";
        return;
    }

    std::ofstream out("downloaded_" + filename, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Failed to create file for writing.\n";
        return;
    }

    char buffer[1024];
    int received = 0;

    while (received < file_size)
    {
        int bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes <= 0)
            break;
        out.write(buffer, bytes);
        received += bytes;
    }

    std::cout << "Downloaded file: downloaded_" << filename << "\n";
}


void uploadFile(SOCKET sock, const std::string &filename)
{
    std::ifstream in(filename, std::ios::binary | std::ios::ate);
    if (!in)
    {
        std::cout << "File not found: " << filename << "\n";
        return;
    }

    // Wait for server response (OK or error)
    char response[1024];
    int resp_bytes = recv(sock, response, sizeof(response), 0);
    response[resp_bytes] = '\0';

    if (std::string(response).rfind("ERROR", 0) == 0)
    {
        std::cout << "Server error: " << response;
        return;
    }

    // Continue with upload
    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::string size_str = std::to_string(size);
    send(sock, size_str.c_str(), size_str.size(), 0);
    Sleep(100); // Optional

    char buffer[1024];
    while (!in.eof())
    {
        in.read(buffer, sizeof(buffer));
        send(sock, buffer, in.gcount(), 0);
    }

    // Wait for confirmation from server
    resp_bytes = recv(sock, response, sizeof(response), 0);
    response[resp_bytes] = '\0';
    std::cout << "Server: " << response;
}


int main()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    // Change to Linux server IP on deployment

    connect(sock, (struct sockaddr *)&server, sizeof(server));

    while (true)
    {
        std::string command;
        std::cout << "ftp> ";
        std::getline(std::cin, command);

        if (command == "LIST")
        {
            sendCommand(sock, "LIST");
            receiveAndPrint(sock);
        }
        else if (command.rfind("GET ", 0) == 0)
        {
            std::string filename = command.substr(4);
            if (filename.empty())
            {
                std::cout << "Usage: GET <filename>\n";
                continue;
            }
            sendCommand(sock, command);
            downloadFile(sock, filename);
        }
        else if (command.rfind("PUT ", 0) == 0)
        {
            std::string filename = command.substr(4);
            if (filename.empty())
            {
                std::cout << "Usage: PUT <filename>\n";
                continue;
            }
            sendCommand(sock, command);
            uploadFile(sock, filename);
        }
        else if (command == "HELP")
        {
            std::cout << "Available commands:\n";
            std::cout << "  LIST           - list files on server\n";
            std::cout << "  GET <file>     - download file\n";
            std::cout << "  PUT <file>     - upload file\n";
            std::cout << "  QUIT           - exit FTP client\n";
        }

        else if (command == "QUIT")
        {
            sendCommand(sock, "QUIT");
            break;
        }
        else
        {
            std::cout << "Unknown command\n";
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
