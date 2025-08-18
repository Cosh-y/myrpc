#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <unistd.h>
#include "provider.h"
#include "coroutine.h"

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
    // void add_event(int fd, event ev);
    // void del_event(int fd);

    static void set_this(scheduler *sched);
    static scheduler* get_this();

private:
    void init_cort_pool();
    std::pair<int, coroutine &> alloc_cort();
    coroutine & get_cort(int idx);

private:
    int m_epfd;
    uint32_t m_cort_pool_size = 10;
    std::vector<std::pair<bool, coroutine>> m_cort_pool;
};

#endif