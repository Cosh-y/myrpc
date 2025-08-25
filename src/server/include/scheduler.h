#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <unistd.h>
#include <memory>
#include "provider.h"
#include "coroutine.h"
#include "timer.h"

enum class event {
    READ,
    WRITE,
};

struct event_data {
    int fd;
    int cort_idx;
};

class scheduler {
public:
    scheduler();
    ~scheduler() {
        close(m_epfd);
    }

    void run();
    connection & add_client(int client_sock);
    void fresh_in_time_wheel(connection & conn);
    // void add_event(int fd, event ev);
    // void del_event(int fd);

    static void set_this(scheduler *sched);
    static scheduler* get_this();

    void return_cort(coroutine & co);

private:
    void init_cort_pool();
    coroutine & alloc_cort();
    coroutine & get_cort(int idx);

private:
    int m_epfd;
    timer m_timer;
    uint32_t m_cort_pool_size = 10;
    std::vector<std::pair<bool, coroutine>> m_cort_pool;
    std::map<int, connection> m_conn_pool;
};

#endif