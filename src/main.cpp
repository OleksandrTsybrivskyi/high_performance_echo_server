#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 1024;
constexpr int BUFFER_SIZE = 4096;
constexpr int THREADS = 4;

// ---------------- Thread pool ----------------

std::queue<int> tasks;
std::mutex mtx;
std::condition_variable cv;
bool running = true;

void worker() {
    char buffer[BUFFER_SIZE];

    while (running) {
        int fd;

        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return !tasks.empty() || !running; });

            if (!running) return;

            fd = tasks.front();
            tasks.pop();
        }

        while (true) {
            ssize_t count = read(fd, buffer, BUFFER_SIZE);

            if (count <= 0) {
                close(fd);
                break;
            }

            ssize_t written = 0;
            while (written < count) {
                ssize_t w = write(fd, buffer + written, count - written);
                if (w <= 0) break;
                written += w;
            }
        }
    }
}

// ---------------- util ----------------

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// ---------------- main ----------------

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

    // start workers
    std::vector<std::thread> pool;
    for (int i = 0; i < THREADS; i++)
        pool.emplace_back(worker);

    while (true) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {
                while (true) {
                    int client_fd = accept(server_fd, nullptr, nullptr);
                    if (client_fd == -1) break;

                    set_nonblocking(client_fd);

                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        tasks.push(client_fd);
                    }
                    cv.notify_one();
                }
            }
        }
    }

    running = false;
    cv.notify_all();

    for (auto &t : pool)
        t.join();

    close(server_fd);
    return 0;
}