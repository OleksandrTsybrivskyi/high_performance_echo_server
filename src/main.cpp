#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd >= 0)
    {
        std::cout << "File oppened successfully\n";
    }
    else
    {
        std::cout << "File Open fail\n";
        return 0;
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);
    while (true)
    {
        int client_fd = accept(server_fd, nullptr, nullptr);
        char buffer[1024];
        int bytes_read = read(client_fd, buffer, sizeof(buffer));
        write(client_fd, buffer, bytes_read);
        close(client_fd);
    }


    return 0;
}