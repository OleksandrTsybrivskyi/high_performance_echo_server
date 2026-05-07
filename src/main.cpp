#include <iostream>
#include <vector>
#include <thread>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <pthread.h>

#define MAX_EVENTS 10000
#define PORT 8080
#define BUFFER_SIZE 4096

// Встановлення неблокуючого режиму
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Робочий потік (один на кожне ядро)
void worker_thread(int thread_id) {
    // 1. CPU Pinning (Прив'язка потоку до конкретного ядра)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    // 2. Memory Pool: Статичний буфер на рівні потоку (без динамічної алокації new/malloc)
    char buffer[BUFFER_SIZE];

    // 3. Створення сокета
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // КЛЮЧОВИЙ МОМЕНТ: SO_REUSEPORT дозволяє багатьом потокам слухати один порт
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    set_nonblocking(listen_fd);
    listen(listen_fd, SOMAXCONN);

    // 4. Epoll (окремий інстанс для кожного потоку)
    int epoll_fd = epoll_create1(0);
    struct epoll_event ev, events[MAX_EVENTS];
    
    ev.events = EPOLLIN | EPOLLET; // Edge-triggered
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == listen_fd) {
                // Приймаємо всі нові з'єднання (EAGAIN loop)
                while (true) {
                    int conn_fd = accept(listen_fd, nullptr, nullptr);
                    if (conn_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        break;
                    }
                    set_nonblocking(conn_fd);
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = conn_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev);
                }
            } else {
                // Обробка даних клієнта
                int fd = events[i].data.fd;
                while (true) {
                    ssize_t n = read(fd, buffer, sizeof(buffer));
                    if (n > 0) {
                        // MSG_NOSIGNAL запобігає падінню сервера (SIGPIPE), якщо клієнт раптово відключився
                        send(fd, buffer, n, MSG_NOSIGNAL); 
                    } else if (n == 0) {
                        close(fd);
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        close(fd);
                        break;
                    }
                }
            }
        }
    }
}

int main() {
    // Визначаємо кількість апаратних ядер
    int num_cores = std::thread::hardware_concurrency();
    if (num_cores == 0) num_cores = 4; // Fallback

    std::cout << "Starting high-performance echo server on port " << PORT << "..." << std::endl;
    std::cout << "Spawning " << num_cores << " independent threads with SO_REUSEPORT." << std::endl;

    std::vector<std::thread> threads;
    
    // Запускаємо по одному потоку на кожне ядро
    for (int i = 0; i < num_cores; ++i) {
        threads.emplace_back(worker_thread, i);
    }

    // Чекаємо завершення (нескінченно)
    for (auto& t : threads) {
        t.join();
    }

    return 0;
}