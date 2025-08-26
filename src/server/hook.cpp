#include <functional>
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <iostream>

#include "hook.h"
#include "scheduler.h"
#include "coroutine.h"

// 为原始的 read/write 函数创建别名，避免名称冲突
// 采用 dlsym 在运行时获取动态链接库中的 io 函数地址
// original_## io ##_t 是在 hook.h 中定义的函数指针类型的类型别名
// 这里定义的 io 函数指针类型的变量，指向原始 io 函数地址，后续用于传入 hook_io 中
original_read_t original_read = (original_read_t)dlsym(RTLD_NEXT, "read");
original_write_t original_write = (original_write_t)dlsym(RTLD_NEXT, "write");
original_recv_t original_recv = (original_recv_t)dlsym(RTLD_NEXT, "recv");
original_send_t original_send = (original_send_t)dlsym(RTLD_NEXT, "send");

bool is_socket(int fd) {
    struct stat st;
    if (fstat(fd, &st) == -1) {
        return false; // fd 无效
    }
    return S_ISSOCK(st.st_mode);
}

template<typename F, typename... Args>  // 可变参数模板，Args 模板参数包，args 函数参数包
int hook_io(int fd, event ev, F origin_io, Args&&... args) {
    // 如果 fd 是 sock，查看其 io 操作是否能立即完成
    if (is_socket(fd)) {
    retry:
        int r = origin_io(fd, std::forward<Args>(args)...);
        coroutine *co = coroutine::get_current();
        // scheduler *sc = scheduler::get_this();
        if (r < 0) {
            if (errno == EAGAIN) { // hook 里判断“榨干”与否，连接里只需要判断读完 size 与否
                // “如果 io 操作无法立即完成就注册 event” 是一种设计，但目前不采用这种设计
                // 比如不会在这里 sc->add_event(fd, ev);
                // 实际上，该协程绑定的连接的 sock 上的 event 会在创建连接时被注册
                // TODO：关于销毁连接；可能的其他 event？
                co->yield();
                goto retry;
            } else if (errno == ECONNRESET) { // 说明对端已经关闭连接了，而且这种关闭是比较粗暴，和返回值为 0 表示的那种不一样
                // TODO：这种异常需要更好的处理或者避免吗？
                std::cout << "errno: " << errno << "\n";
                co->yield();
                exit(EXIT_FAILURE);
            } else {
                std::cout << "errno: " << errno << "\n";
                perror("hook_io");
                exit(EXIT_FAILURE);
            }
        } else {
            // sc->del_event(fd);
            return r;
        }
    } else {
        // 如果不是 socket，直接调用原始 io 操作
        return origin_io(fd, std::forward<Args>(args)...);
    }
}

ssize_t read(int fd, void *buf, size_t count) {
    return hook_io(fd, event::READ, original_read, buf, count);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return hook_io(fd, event::WRITE, original_write, buf, count);
}

ssize_t recv(int fd, void *buf, size_t len, int flags) {
    return hook_io(fd, event::READ, original_recv, buf, len, flags);
}

ssize_t send(int fd, const void *buf, size_t len, int flags) {
    return hook_io(fd, event::WRITE, original_send, buf, len, flags);
}
