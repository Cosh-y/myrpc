#include <functional>
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "hook.h"
#include "scheduler.h"

// 为原始的 read/write 函数创建别名，避免名称冲突
// 采用 dlsym 在运行时获取动态链接库中的 io 函数地址
// original_## io ##_t 是在 hook.h 中定义的函数指针类型的类型别名
// 这里定义的 io 函数指针类型的变量，指向原始 io 函数地址，后续用于传入 hook_io 中
original_read_t original_read = (original_read_t)dlsym(RTLD_NEXT, "read");
original_write_t original_write = (original_write_t)dlsym(RTLD_NEXT, "write");

bool is_socket(int fd) {
    struct stat st;
    if (fstat(fd, &st) == -1) {
        return false; // fd 无效
    }
    return S_ISSOCK(st.st_mode);
}

template<typename F, typename... Args>
int hook_io(int fd, event ev, F origin_io, Args&&... args) {
    // 如果 fd 是 sock，查看其 io 操作是否能立即完成
    if (is_socket(fd)) {
    retry:
        int r = origin_io(fd, std::forward<Args>(args)...);
        coroutine& co = coroutine::get_current();
        scheduler *sc = scheduler::get_this();
        if (-1 == r) {
            // 如果 io 操作无法立即完成，注册 event，yield 协程，retry
            sc->add_event(fd, ev);
            co.yield();
            goto retry;
        } else {
            sc->del_event(fd);
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
