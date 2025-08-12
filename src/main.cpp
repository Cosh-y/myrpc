#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <map>
#include "coroutine.h"

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);  // tcp_socket
    if (sock == -1)
    {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
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
        return 1;
    }

    r = listen(sock, 1024); // 设置监听队列长度为1024
    if (r == -1)
    {
        std::cerr << "Failed to listen on socket" << std::endl;
        return 1;
    }
    fcntl(sock, F_SETFL, O_NONBLOCK); // 将 sock 设置为非阻塞，使 accept() 不会阻塞

    int epfd = epoll_create1(0);
    if (epfd == -1)
    {
        std::cerr << "Failed to create epoll file descriptor" << std::endl;
        return 1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev);

    std::map<int, coroutine*> co_map;

    while (true) {
        struct epoll_event events[10];
        std::cout << "Waiting for events..." << std::endl;
        int nfds = epoll_wait(epfd, events, 10, 5000); // 等待事件发生
        if (nfds == -1)
        {
            std::cerr << "epoll_wait failed" << std::endl;
            return 1;
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
                // fcntl(client_sock, F_SETFL, O_NONBLOCK); // 将 client_sock 设置为非阻塞
                struct epoll_event client_ev;
                // 关于触发模式：epoll 触发模式有两种，边沿和水平
                // 边沿模式对于可读、可写状态只通知一次，必须在当此完成全部读写，编程难度比较大
                client_ev.events = EPOLLIN;
                client_ev.data.fd = client_sock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_sock, &client_ev);
            } else {
                // 处理已连接的客户端
                if (co_map.find(events[i].data.fd) == co_map.end()) {
                    // 创建新的协程
                    coroutine* co = new coroutine([events, i]() {
                        char buffer[1024];
                        ssize_t bytes_read = read(events[i].data.fd, buffer, sizeof(buffer));
                        if (bytes_read <= 0) {
                            // 客户端关闭连接或读取错误
                            std::cout << "Client disconnected or read error" << std::endl;
                            close(events[i].data.fd);
                        } else {
                            // 处理读取的数据
                            std::cout << "Received data: " << std::string(buffer, bytes_read) << std::endl;
                        }
                    });
                    co_map[events[i].data.fd] = co;
                }
                co_map[events[i].data.fd]->resume();
            }
        }
    }
}