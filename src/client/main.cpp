#include <string>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sstream>
#include <iostream>
#include <unistd.h>

#include "test.pb.h"

enum class state {
    CONNECTING,
    IDLE,
    SENDING,
    RECV_PS,
    RECV_PKG,
    CLOSED,
};

struct conn {
    int fd_sock;

    state state_;

    std::string send_buf;
    int send_off;
    int to_send;

    std::string recv_buf;
    int recv_off;
    int to_recv;

    uint32_t count {0}; // 本连接发送的请求数
    uint32_t id;
};

void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) exit(EXIT_FAILURE);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int create_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    set_nonblock(sock);
    return sock;
}

void make_sockaddr_in(sockaddr_in & addr, const std::string & ip, uint16_t port) {
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
}

void prepare_conn(conn & c, int c_id);

void connect_server(conn & c, const std::string & ip, uint16_t port) {
    sockaddr_in addr;
    make_sockaddr_in(addr, ip, port);

    int r = connect(c.fd_sock, (sockaddr *) &addr, sizeof(addr)); // cpp 的强制类型转换，有时候我好像记不住强制类型转换。
    if (r == 0) {
        c.state_ = state::IDLE;
        prepare_conn(c, c.id);
    } else if (r < 0 && errno == EINPROGRESS) {
        c.state_ = state::CONNECTING;
    } else {
        perror("connect error");
        exit(EXIT_FAILURE);
    }
}

void prepare_conn(conn & c, int c_id) {
    c.send_off = 0;
    
    test::EchoRequest req;
    req.set_id(c_id);
    req.set_content("Hello, RPC; req " + std::to_string(c.count++));
    std::string req_str = req.SerializeAsString();
    
    test::RpcHeader header;
    header.set_service("EchoService");
    header.set_method("Echo");
    header.set_pkg_size(req_str.size());
    std::string hed_str = header.SerializeAsString();
    uint32_t hed_size = hed_str.size();

    std::stringstream stream;
    stream.write((char *)&hed_size, sizeof(uint32_t)); // 确保头部大小
    c.send_buf = stream.str() + hed_str + req_str;
    c.to_send = c.send_buf.size();
    c.state_ = state::SENDING;
}

void print_retval(const std::string & val_str) {
    test::EchoResponse resp;
    resp.ParseFromString(val_str);
    std::cout << "val_str: " << val_str << "\n";
    std::cout << "resp id (conn id): " << resp.id() << "; resp.content: " << resp.content() << std::endl;
}

int main(int argc, char **args) {
    std::string ip = (argc > 1) ? args[1] : "192.168.106.205";
    uint16_t port  = (argc > 2) ? (std::stoi(args[2])) : 8080;  // 字符串转整数 stoi
    uint32_t n_conns = (argc > 3) ? std::stoi(args[3]) : 10;
    uint32_t n_reqs  = (argc > 4) ? std::stoi(args[4]) : 10000;

    std::vector<conn> conns;
    conns.reserve(1024);
    int epfd = epoll_create1(0);

    for (int i = 0;i < n_conns;i++) {
        conn c; c.id = i;
        c.fd_sock = create_socket();
        connect_server(c, ip, port);

        epoll_event ev;
        ev.data.u32 = i;  // conn id 记录到 event 里
        // 这里 events 为 EPOLLIN | EPOLLOUT | EPOLLET 是很激进的
        // 后续状态机的设计，稍不注意就会出错，关键在于读写循环必须至少“榨干”读/写能力中的一个
        // 才能保证连接的状态机有机会被再次唤醒！
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        epoll_ctl(epfd, EPOLL_CTL_ADD, c.fd_sock, &ev);

        conns.push_back(std::move(c));
    }

    int n_send = 0;
    int n_recv = 0;
    std::vector<epoll_event> events(1024);

    auto t0 = std::chrono::steady_clock::now();
    bool over = false;
    while (!over) {
        std::cout << "epoll waiting!" << "\n";
        int n = epoll_wait(epfd, events.data(), 1024, 5000);
        std::cout << "gets events: " << n << "\n";
        for (int i = 0;i < n;i++) {
            epoll_event & ev = events[i];
            int idx = ev.data.u32;
            conn & c = conns[idx];

            // 状态机，自定义二进制协议客户端处理流程
            if (c.state_ == state::CONNECTING) {
                int serr = 0; socklen_t slen = sizeof(serr);
                getsockopt(c.fd_sock, SOL_SOCKET, SO_ERROR, &serr, &slen);
                if (serr != 0) {
                    c.state_ = state::CLOSED;
                    continue;
                }
                c.state_ = state::IDLE;
                prepare_conn(c, idx);
            }

        // 使用了 goto 语句，可能使得代码有些难懂，这里解释一下：这里的 next_req 是为了在处理完上一个请求的发送、接收后
        // 立刻在当前连接上处理下一个请求，为了“榨干”当前连接的可读/可写状态（因为 epoll 用的边沿触发）。
        next_req:
            if (c.state_ == state::SENDING) {
                // 这个循环有讲究，如何发送尽可能多的数据
                // 用 send_off 管理已经发送的数据数量
                // 当 errno 为 EAGAIN 时，说明 fd 被“榨干”了
                // 即此非阻塞的 fd 因为正常原因暂时不能再读或者写了，下次可读/写会重新触发 epoll_wait
                while (true) {
                    int r = send(c.fd_sock, c.send_buf.data() + c.send_off, c.to_send - c.send_off, 0);
                    if (r < 0) {
                        if (errno == EAGAIN) {   // 判断是否写到了不可再写的地步
                            std::cout << "unwriteable!" << "\n";
                            break;
                        } else {
                            c.state_ = state::CLOSED;
                            break;
                        }
                    }
                    c.send_off += r;
                    if (c.send_off == c.to_send) {
                        n_send++;
                        c.state_ = state::RECV_PS;
                        c.recv_off = 0;
                        c.to_recv = sizeof(uint32_t);
                        // 这里不能用 reserve，如果展开来说，这是关于用 string 当 buf 接收网络数据的一大坑点！
                        // 不必多预留一字节的'\0'
                        // 接收几字节就 resize 成几字节，否则后面的自动填入的 0 也会导致后续的解析错误
                        // 要知道 string 这东西有两个属性 size 和 capacity，而接收网络数据时 size 是不会自动扩展的。
                        // 因为你给 recv 的是 str.data() 是个裸指针！还挺危险的好像！
                        // 平常使用 string 时之所以不担心这个问题，是因为我们通过 string 的方法修改 string，方法内部
                        // 自动维护了 size。而这里 recv 直接写 string.data() 裸指针，string 的 size 不会改的。
                        c.recv_buf.resize(c.to_recv);
                        break;
                    } else {
                        continue;
                    }
                }
            }

            if (c.state_ == state::RECV_PS) {
                while (true) {
                    int r = recv(c.fd_sock, c.recv_buf.data() + c.recv_off, c.to_recv - c.recv_off, 0);
                    if (r < 0) {
                        if (errno == EAGAIN) {
                            std::cout << "unreadable!1" << "\n"; // 有点傻的写法hh
                            break;
                        } else {
                            c.state_ = state::CLOSED;
                            break;
                        }
                    }
                    c.recv_off += r;
                    if (c.recv_off == c.to_recv) {
                        c.state_ = state::RECV_PKG;
                        c.recv_off = 0;
                        c.to_recv = *(uint32_t *)c.recv_buf.data();
                        c.recv_buf.resize(c.to_recv);
                        break;
                    } else {
                        continue;
                    }
                }
            }

            if (c.state_ == state::RECV_PKG) {
                while (true) {
                    int r = recv(c.fd_sock, c.recv_buf.data() + c.recv_off, c.to_recv - c.recv_off, 0);
                    if (r < 0) {
                        if (errno == EAGAIN) {
                            std::cout << "unreadable!2" << "\n";
                            break;
                        } else {
                            c.state_ = state::CLOSED;
                            break;
                        }
                    }
                    c.recv_off += r;
                    if (c.recv_off == c.to_recv) {
                        n_recv++;
                        print_retval(c.recv_buf);
                        if (n_send >= n_reqs) {
                            c.state_ = state::CLOSED;
                            break;
                        } else {
                            c.state_ = state::IDLE;
                            prepare_conn(c, c.id);
                            goto next_req;
                        }
                    } else {
                        continue;
                    }
                }
            }

            if (c.state_ == state::CLOSED) {
                std::cout << "Connection closed for conn id: " << c.id << "\n";
                close(c.fd_sock);
                c.fd_sock = -1;
                if (n_send >= n_reqs) {
                    over = true;
                }
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(t1 - std::chrono::steady_clock::now() + (t1 - t1)).count(); // 防止优化警告
    sec = std::chrono::duration<double>(t1 - std::chrono::steady_clock::now() + (t1 - t0)).count(); // 修正
    sec = std::chrono::duration<double>(t1 - t0).count();

    // 清理
    for (auto& c : conns) {
        if (c.fd_sock >= 0) close(c.fd_sock);
    }
    close(epfd);

    std::cout << "Expected requests: " << n_reqs << "\n";
    std::cout << "Actually requests: " << n_send << "\n";
    std::cout << "Completed OK : " << n_recv << "\n";
    std::cout << "Elapsed (s)  : " << sec << "\n";
    std::cout << "QPS          : " << (sec > 0 ? (n_recv / sec) : 0) << "\n";

    return 0;

}