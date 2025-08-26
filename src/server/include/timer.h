#ifndef TIMER_H
#define TIMER_H

#include <queue>
#include <vector>
#include <memory>

class connection;

class time_wheel {
public:
    time_wheel(int n_spokes, int update_cycle);
    void fresh(connection & conn);
    void rotate();

private:
    // 原本使用了 std::queue<std::vector<connection &>> 这种类型，无法通过编译
    // 原来是标准容器内不能存放引用类型
    std::queue<std::vector<connection *>> m_wheel;
    int m_n_spokes;
    int m_update_cycle;
};

class timer {
public:
    timer();
    void on_time();
    int get_fd();
    void fresh(connection & conn);

private:
    int m_fd_timer;
    time_wheel m_time_wheel; // 时间轮对象
};

#endif
