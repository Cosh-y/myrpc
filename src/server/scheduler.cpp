#include <poll.h>
#include <sys/epoll.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <map>
#include "coroutine.h"
#include "connection.h"
#include "provider.h"
#include "scheduler.h"

#include "test.pb.h"

static bool debug = true;

// utils function
static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) exit(EXIT_FAILURE);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int check_socket_status(int sockfd) {
    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN | POLLOUT;

    int ret = poll(&pfd, 1, 0); // timeout 0ms，非阻塞
    if (ret < 0) {
        perror("poll");
        return -1;
    }

    if (pfd.revents & POLLIN) {
        printf("socket is readable\n");
    }
    if (pfd.revents & POLLOUT) {
        printf("socket is writable\n");
    }
    return ret;
}

// 不要在头文件中定义这种变量，在这里定义比较合理
static thread_local scheduler *t_scheduler = nullptr;

scheduler::scheduler() {
    m_epfd = epoll_create1(0);
    if (m_epfd == -1) {
        std::cerr << "Failed to create epoll file descriptor" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 默认构造 m_timer;

    init_cort_pool();
    set_this(this);  // 设置当前调度器实例
}

void scheduler::init_cort_pool() {
    for (uint32_t i = 0;i < m_cort_pool_size;i++) {
        // coroutine(i) 返回一个临时对象，后续可能会触发移动语义
        m_cort_pool.emplace_back(true, coroutine(i));
    }
}

coroutine & scheduler::alloc_cort() {
    for (uint32_t i = 0;i < m_cort_pool.size();i++) {
        if (m_cort_pool[i].first == true) {
            m_cort_pool[i].first = false;
            if (debug) {
                std::cout << "alloc cort " << i << "\n";
            }
            return m_cort_pool[i].second;
        }
    }
    std::cerr << "no free coroutine!";
    exit(EXIT_FAILURE);
}

void scheduler::return_cort(coroutine & co) {
    int co_id = co.id();
    assert(!m_cort_pool[co_id].first);

    co.reset_uctx();
    m_cort_pool[co_id].first = true; // 空闲
}

coroutine & scheduler::get_cort(int idx) {
    return m_cort_pool[idx].second;
}

void scheduler::run() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);  // tcp_socket
    if (sock == -1)
    {
        std::cerr << "Failed to create socket" << std::endl;
        exit(EXIT_FAILURE);
    }
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    
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

    struct epoll_event accept_ev;
    accept_ev.events = EPOLLIN; 
    m_evds[0].fd = sock;
    accept_ev.data.ptr = &m_evds[0];
    epoll_ctl(m_epfd, EPOLL_CTL_ADD, sock, &accept_ev);

    struct epoll_event time_ev;
    time_ev.events = EPOLLIN; 
    m_evds[1].fd = m_timer.get_fd();
    time_ev.data.ptr = &m_evds[1];
    epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_timer.get_fd(), &time_ev);

    while (true) {
        struct epoll_event events[1024];
        std::cout << "Waiting for events..." << std::endl;
        int nfds = epoll_wait(m_epfd, events, 10, 5000); // 等待事件发生
        if (nfds == -1)
        {
            std::cerr << "epoll_wait failed" << std::endl;
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; ++i) {
            struct event_data *ev_data = (event_data *)events[i].data.ptr;
            if (ev_data->fd == sock) {
                // 有新的连接到来
                std::cout << "New connection detected" << std::endl;
                int client_sock = accept(sock, nullptr, nullptr);
                if (client_sock == -1) {
                    std::cerr << "Failed to accept connection" << std::endl;
                    continue;
                }
                set_nonblocking(client_sock); // 将 client_sock 设置为非阻塞

                coroutine & co = alloc_cort();

                epoll_event ev;
                // 这段 堆内存 传入 conn，与 conn 一同管理一同释放，是否需要智能指针？
                ev.data.ptr = new event_data { client_sock, co.id() };
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                epoll_ctl(m_epfd, EPOLL_CTL_ADD, client_sock, &ev);

                connection & conn = add_client(client_sock);
                fresh_in_time_wheel(conn);
                co.set_connection(conn);
                conn.set_coroutine(co);
                conn.set_evd_ptr((struct event_data *) ev.data.ptr);
                co.resume();
            } else if (ev_data->fd == m_timer.get_fd()) {
                // 定时器到时
                m_timer.on_time();
            } else {
                // 处理已连接的客户端
                coroutine & co = get_cort(ev_data->cort_idx);
                if (debug) { 
                    std::cout << "resumed co idx: " << ev_data->cort_idx << "; co's conn's id: " << co.get_connection_id() << "\n";
                }
                co.resume();
                if (debug) {
                    std::cout << "resumed co yield, co idx: " << co.id() << "\n";
                }
            }
        }
    }
}

// TODO: 使用 try_emplace 一次查找搞定
connection & scheduler::add_client(int client_sock) {
    auto it = m_conn_pool.find(client_sock);
    if (it != m_conn_pool.end()) {
        // 说明原来用于连接的某个 fd 被 close 了
        it->second.reset_with_sock(client_sock);
        return it->second;
    } else {
        m_conn_pool.emplace(client_sock, connection(client_sock));
        return m_conn_pool[client_sock];
    }
}

void scheduler::fresh_in_time_wheel(connection & conn) {
    m_timer.fresh(conn);
}

// void scheduler::add_event(int fd, event ev) {
//     struct epoll_event e;
//     // 关于触发模式：epoll 触发模式有两种，边沿和水平
//     // 边沿模式对于可读、可写状态只通知一次，必须在当此完成全部读写，编程难度比较大
//     e.events = (ev == event::READ) ? EPOLLIN : EPOLLOUT;
//     e.data.fd = fd;
//     epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &e);
// }

// void scheduler::del_event(int fd) {
//     epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
// }

void scheduler::set_this(scheduler *sched) {
    t_scheduler = sched;
}

scheduler* scheduler::get_this() {
    return t_scheduler;
}
