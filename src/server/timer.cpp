#include <vector>
#include <memory>
#include <sys/timerfd.h>
#include <iostream>
#include <unistd.h>

#include "timer.h"
#include "coroutine.h"

static bool debug = true;

time_wheel::time_wheel(int n_spokes, int update_cycle) : m_n_spokes(n_spokes), m_update_cycle(update_cycle) {
    for (int i = 0;i < m_n_spokes;i++) {
        m_wheel.push(std::vector<connection *>());
    }
}

void time_wheel::fresh(connection & conn) {
    if (conn.freshable()) {
        conn.fresh();
        m_wheel.back().emplace_back(&conn);
    }
}

void time_wheel::rotate() {
    std::vector<connection *> tmp = m_wheel.front();
    m_wheel.pop();
    m_wheel.push(std::vector<connection *>());
    if (debug) {
        if (!tmp.empty()) {
            std::cout << "not empty spoke:\n";
        } else {
            std::cout << "empty spoke\n";
        }
    }
    for (connection * conn : tmp) {
        conn->outdate();
        if (!conn->freshed()) {
            if (debug) {
                std::cout << "conn " << conn->id() << " timeout\n";
                std::cout << "cort " << conn->get_coroutine()->id() << " returned\n";
            }
            conn->timeout();
        }
    }
}

timer::timer() : m_time_wheel(2, 10) {
    m_fd_timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec timer_set;
    timer_set.it_value.tv_sec = 1;
    timer_set.it_interval.tv_sec = 1;
    timerfd_settime(m_fd_timer, 0, &timer_set, NULL);
}

int timer::get_fd() {
    return m_fd_timer;
}

void timer::on_time() {
    std::cout << "in timer::on_time()\n";
    uint64_t exp;
    read(m_fd_timer, &exp, sizeof(exp)); // 必须读掉，从而重置计时器的可读状态。
    m_time_wheel.rotate();
}

void timer::fresh(connection & conn) {
    m_time_wheel.fresh(conn);
}
