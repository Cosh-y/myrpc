#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <unistd.h>
#include <memory>
#include <map>

#include "timer.h"
#include "coroutine.h"
#include "event.h"

class worker;

enum class event {
    READ,
    WRITE,
};

class scheduler {
public:
    scheduler();
    ~scheduler() {
        close(m_epfd);
    }

    void run();

    static void set_this(scheduler *sched);
    static scheduler* get_this();

private:
    int m_epfd;
    struct event_data m_evds[2];
};

#endif