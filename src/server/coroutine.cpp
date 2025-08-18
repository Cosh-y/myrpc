#include <iostream>
#include "coroutine.h"

static thread_local coroutine *t_cur_coroutine;
static thread_local ucontext_t t_main_ctx;

coroutine::coroutine() {
    m_stack_size = 1024 * 1024;
    m_stack_ptr = new char[m_stack_size];
    getcontext(&m_ctx);
    m_ctx.uc_stack.ss_sp = m_stack_ptr;
    m_ctx.uc_stack.ss_size = m_stack_size;
    makecontext(&m_ctx, &coroutine::coroutine_main, 0);
}

coroutine::coroutine(coroutine && co) {
    m_stack_size = co.m_stack_size;
    m_stack_ptr = co.m_stack_ptr;
    co.m_stack_ptr = nullptr;
    m_conn = co.m_conn;
    m_ctx = co.m_ctx;
}

coroutine* coroutine::get_current() {
    return t_cur_coroutine;
}

void coroutine::set_current(coroutine& co) {
    t_cur_coroutine = &co;
}

void coroutine::resume() {
    set_current(*this); // 将要 resume 的协程设置为当前协程
    swapcontext(&t_main_ctx, &m_ctx);
}

void coroutine::yield() {
    swapcontext(&m_ctx, &t_main_ctx);
}

coroutine::coroutine(std::function<void()> cb) : m_cb(std::move(cb)) {
    m_stack_size = 1024 * 1024;
    m_stack_ptr = new char[m_stack_size];
    getcontext(&m_ctx);
    m_ctx.uc_stack.ss_sp = m_stack_ptr;
    m_ctx.uc_stack.ss_size = m_stack_size;
    makecontext(&m_ctx, &coroutine::coroutine_main, 0);
}

void coroutine::set_connection(connection && conn) {
    m_conn = std::move(conn);
    m_cb = std::bind(&connection::run, &m_conn);
}

int coroutine::get_connection_id() {
    return m_conn.id();
}

void coroutine::coroutine_main() {
    coroutine *co = get_current();
    co->m_cb();
    co->yield();
}

coroutine::~coroutine() {
    delete[] m_stack_ptr;
}
