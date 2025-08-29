#include <iostream>
#include <sys/epoll.h>
#include <assert.h>

#include "worker.h"
#include "coroutine.h"

static thread_local worker *t_worker = nullptr;

static bool debug = true;

coroutine_pool::coroutine_pool(int size) {
    for (int i = 0;i < size;i++) {
        m_pool.emplace_back(true, coroutine(i));
    }
}

coroutine & coroutine_pool::alloc_cort() {
    for (uint32_t i = 0;i < m_pool.size();i++) {
        if (m_pool[i].first == true) {
            m_pool[i].first = false;
            if (debug) {
                std::cout << "alloc cort " << i << "\n";
            }
            return m_pool[i].second;
        }
    }
    std::cerr << "no free coroutine!";
    exit(EXIT_FAILURE);
}

void coroutine_pool::return_cort(coroutine & co) {
    int co_id = co.id();
    assert(!m_pool[co_id].first);

    co.reset_uctx();
    m_pool[co_id].first = true; // 空闲
}

coroutine & coroutine_pool::get_cort(int idx) {
    return m_pool[idx].second;
}

// 1. 成员初始化列表的初始化方式（但是要注意启动线程的时机）
worker::worker() : /*m_thread(&worker::run, this),*/ m_cort_pool(12) {
    static int id = 0;
    m_id = ++id;
    // 2. 构造临时匿名对象再赋值
    // 这种写法是正确的，&worker::run 这种语法【&类名::方法名】取出成员函数指针
    // thread 构造函数能判断第一个参数是成员函数指针，此时第二个参数作为调用成员函数的对象，而非一般的函数参数
    // m_thread = std::thread(&worker::run, this);

    // 3. 
    // 这种写法也是正确的，绕开了成员函数指针的问题
    // m_thread = std::thread([this] () {
    //     this->run();
    // });

    // 4. 
    // 这种写法也是正确的，怎么知道 run 是成员函数的？
    // 捕获 this 的 lambda 比较特殊，捕获了 this 的 lambda 内部就仿佛类的一般的成员函数内部一样
    // 可以直接访问类的（包括私有的）成员和函数
    // m_thread = std::thread([this] () {
    //     run();
    // });
    m_epfd = epoll_create1(0);
    
    // 默认构造 m_timer
    m_thread = std::thread(&worker::run, this);
}

void worker::set_this(worker *w) {
    t_worker = w;
}

worker* worker::get_this() {
    return t_worker;
}

void worker::add_client(int client_sock) {
    std::lock_guard<std::mutex> lock(m_mtx);
    m_pending_fds.push_back(client_sock);
}

void worker::return_cort(coroutine & co) {
    m_cort_pool.return_cort(co);
}

void worker::run() {
    set_this(this);
    std::cout << "start worker " << m_id << "\n";

    struct epoll_event time_ev;
    time_ev.events = EPOLLIN; 
    m_timer_evd.fd = m_timer.get_fd();
    time_ev.data.ptr = &m_timer_evd;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_timer.get_fd(), &time_ev) == -1) {
        perror("epoll_ctl timerfd");
    }

    while (true) {
        while (!m_pending_fds.empty()) {
            int client_sock;
            {
                std::lock_guard<std::mutex> lock(m_mtx);
                client_sock = m_pending_fds.front();
                m_pending_fds.pop_front();
            }
            coroutine & co = m_cort_pool.alloc_cort();

            epoll_event ev;
            // 这段 堆内存 传入 conn，与 conn 一同管理一同释放，是否需要智能指针？
            ev.data.ptr = new event_data { client_sock, co.id() };
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            epoll_ctl(m_epfd, EPOLL_CTL_ADD, client_sock, &ev);

            connection & conn = alloc_conn(client_sock);
            fresh_in_time_wheel(conn);
            co.set_connection(conn);
            conn.set_coroutine(co);
            conn.set_evd_ptr((struct event_data *) ev.data.ptr);
            co.resume();
        }

        struct epoll_event events[1024];
        std::cout << "Worker " << m_id << " is waiting for events..." << std::endl;
        int nfds = epoll_wait(m_epfd, events, 1024, 5000);
        for (int i = 0; i < nfds; ++i) {
            struct event_data *ev_data = (event_data *)events[i].data.ptr;
            if (ev_data->fd == m_timer.get_fd()) {
                // 定时器到时
                m_timer.on_time();
            } else {
                // 处理已连接的客户端
                coroutine & co = m_cort_pool.get_cort(ev_data->cort_idx);
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
connection & worker::alloc_conn(int client_sock) {
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

void worker::fresh_in_time_wheel(connection & conn) {
    m_timer.fresh(conn);
}
