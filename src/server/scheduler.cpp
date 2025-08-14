#include <sys/epoll.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <map>
#include "test.pb.h"
#include "coroutine.h"
#include "provider.h"
#include "scheduler.h"

// utils function
static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) exit(EXIT_FAILURE);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 不要在头文件中定义这种变量，在这里定义比较合理
static thread_local scheduler *t_scheduler = nullptr;

scheduler::scheduler(provider &prov) : m_prov(prov) {
    m_epfd = epoll_create1(0);
    if (m_epfd == -1) {
        std::cerr << "Failed to create epoll file descriptor" << std::endl;
        exit(EXIT_FAILURE);
    }
    set_this(this);  // 设置当前调度器实例
}

void scheduler::run() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);  // tcp_socket
    if (sock == -1)
    {
        std::cerr << "Failed to create socket" << std::endl;
        exit(EXIT_FAILURE);
    }
    
    int r;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // 接收来自所有IP的连接
    addr.sin_port = htons(8080);
    r = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (r == -1)
    {
        std::cerr << "Failed to bind socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    r = listen(sock, 1024); // 设置监听队列长度为1024
    if (r == -1)
    {
        std::cerr << "Failed to listen on socket" << std::endl;
        exit(EXIT_FAILURE);
    }
    set_nonblocking(sock); // 将 sock 设置为非阻塞，使 accept() 不会阻塞

    int epfd = epoll_create1(0);
    if (epfd == -1)
    {
        std::cerr << "Failed to create epoll file descriptor" << std::endl;
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev);

    std::map<int, coroutine*> co_map;

    int wake_times = 0;

    while (true) {
        struct epoll_event events[10];
        std::cout << "Waiting for events..." << std::endl;
        int nfds = epoll_wait(epfd, events, 10, 5000); // 等待事件发生
        if (nfds == -1)
        {
            std::cerr << "epoll_wait failed" << std::endl;
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == sock) {
                // 有新的连接到来
                std::cout << "New connection detected" << std::endl;
                int client_sock = accept(sock, nullptr, nullptr);
                if (client_sock == -1) {
                    std::cerr << "Failed to accept connection" << std::endl;
                    continue;
                }
                set_nonblocking(client_sock); // 将 client_sock 设置为非阻塞
                coroutine *co = new coroutine([client_sock]() {
                    scheduler::get_this()->m_prov.run(client_sock);
                });
                co_map.emplace(client_sock, co);
                co->resume();
            } else {
                // 处理已连接的客户端
                if (co_map.find(events[i].data.fd) == co_map.end()) {
                    std::cerr << "No coroutine found for fd: " << events[i].data.fd << std::endl;
                }
                std::cout << "Resuming coroutine for fd: " << events[i].data.fd << std::endl;
                co_map[events[i].data.fd]->resume();
            }
        }
    }
}

void scheduler::add_event(int fd, event ev) {
    struct epoll_event e;
    // 关于触发模式：epoll 触发模式有两种，边沿和水平
    // 边沿模式对于可读、可写状态只通知一次，必须在当此完成全部读写，编程难度比较大
    e.events = (ev == event::READ) ? EPOLLIN : EPOLLOUT;
    e.data.fd = fd;
    epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &e);
}

void scheduler::del_event(int fd) {
    epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
}

void scheduler::set_this(scheduler *sched) {
    t_scheduler = sched;
}

scheduler* scheduler::get_this() {
    return t_scheduler;
}
