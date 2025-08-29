#ifndef WORKER_H
#define WORKER_H

#include <thread>
#include <vector>
#include <map>
#include <mutex>

#include "timer.h"
#include "event.h"
class coroutine;
class connection;

class coroutine_pool {
public:
    coroutine_pool(int size);
    coroutine & alloc_cort();
    coroutine & get_cort(int idx);
    void return_cort(coroutine & co);

private:
    std::vector<std::pair<bool, coroutine>> m_pool;
};

class worker {    
public:
    worker();
    void fresh_in_time_wheel(connection & conn);
    void add_client(int client_sock);
    void return_cort(coroutine & co);
    
    static worker* get_this();
    
private:
    static void set_this(worker *w);
    connection & alloc_conn(int client_sock);
    void run();

private:
    std::thread m_thread;
    int m_epfd;
    coroutine_pool m_cort_pool;
    std::map<int, connection> m_conn_pool;
    timer m_timer;
    event_data m_timer_evd;

    std::mutex m_mtx;
    std::deque<int> m_pending_fds;
    
    int m_id;
};

#endif
