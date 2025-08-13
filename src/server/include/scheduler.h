#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <unistd.h>
#include "provider.h"
#include "coroutine.h"

enum event {
    READ,
    WRITE,
};

class scheduler {
public:
    scheduler() = default;
    scheduler(provider &prov);
    ~scheduler() {
        close(m_epfd);
    }

    void run();
    void add_event(int fd, event ev);
    void del_event(int fd);

    static void set_this(scheduler *sched);
    static scheduler* get_this();
private:
    int m_epfd;
    provider m_prov;
};

#endif