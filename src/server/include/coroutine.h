#ifndef COROUTINE_H
#define COROUTINE_H

#include <ucontext.h>
#include <functional>

#include "connection.h"

class connection;

class coroutine {
public:
    coroutine() = default;
    coroutine(int id);
    coroutine(std::function<void()> cb);
    coroutine(coroutine && co);
    ~coroutine();
    void set_connection(connection & conn);
    int get_connection_id();
    int id() const { return m_id; }
    void resume();
    void yield();
    static coroutine* get_current();
    static void set_current(coroutine& co);
    static void coroutine_main();
private:
    // 协程的上下文信息
    ucontext_t m_ctx;
    char *m_stack_ptr;
    size_t m_stack_size;

    connection * m_conn;
    int m_id;
    // 参考其他项目的协程实现，cb 的调用特征标都是 void() 暂时不懂为什么
    std::function<void()> m_cb;
};

#endif
