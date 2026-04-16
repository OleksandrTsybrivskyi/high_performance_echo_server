// epoll_echo_server_fixed.cpp

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 1024;
constexpr int BUFFER_SIZE = 4096;

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, SOMAXCONN);

    set_nonblocking(server_fd);

    int epoll_fd = epoll_create1(0);

    epoll_event ev{}, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    while (true) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {
                while (true) {
                    int client_fd = accept(server_fd, nullptr, nullptr);
                    if (client_fd == -1) break;

                    set_nonblocking(client_fd);

                    epoll_event client_ev{};
                    client_ev.events = EPOLLIN | EPOLLET;
                    client_ev.data.fd = client_fd;

                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);
                }
            } 
            else {
                char buffer[BUFFER_SIZE];

                while (true) {
                    ssize_t count = read(fd, buffer, BUFFER_SIZE);

                    if (count == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        close(fd);
                        break;
                    }

                    if (count == 0) {
                        close(fd);
                        break;
                    }

                    // echo back (non-blocking safe write loop)
                    ssize_t total_written = 0;

                    while (total_written < count) {
                        ssize_t w = write(fd, buffer + total_written, count - total_written);

                        if (w == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;
                            close(fd);
                            break;
                        }

                        total_written += w;
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}