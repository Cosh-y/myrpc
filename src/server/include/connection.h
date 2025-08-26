#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>

class coroutine;

class connection {
public:
    connection() = default;
    connection(int fd_sock);
    void set_coroutine(coroutine & cort) { m_cort = &cort; }
    const coroutine * get_coroutine() { return m_cort; }
    void run();
    int id() { return m_id; }
    void reset_with_sock(int sock);
    bool freshable() { return m_n_freshed < 2; }
    bool freshed() { return m_n_freshed > 0; }
    void fresh() { m_n_freshed++; }
    void outdate() { m_n_freshed--; }
    int get_n_freshed() { return m_n_freshed; }
    void timeout();
    void set_evd_ptr(struct event_data *ptr);
    
private:
    enum class state {
        RECV_SIZE,
        RECV_HDR,
        RECV_BODY,
        SEND_RESP,
        CLOSED,
    };
private:
    int m_id;

    int m_fd_sock;

    state m_state;

    std::string m_send_buf;
    uint32_t m_send_off;
    uint32_t m_to_send;

    std::string m_recv_buf;
    uint32_t m_recv_off;
    uint32_t m_to_recv;

    // 记录 conn 的引用在时间轮中的出现次数；当时间轮转动时，如果淘汰的辐条中有此 conn 的引用，就减少这个次数
    // 次数为 0，回收资源；每次 fresh 此 conn 时，就插入新的 conn 引用到时间轮，并增加这个次数；
    // 可以设置一个上限，比如 2 就行，在 fresh 时了解到此 conn 是否已经被 fresh 了；
    int m_n_freshed;

    // 这里我原本使用的是引用，其实应该使用指针。这里想表达的其实是一个临时的绑定关系，conn 和 cort 的绑定是可以改变的
    // 但引用一旦被赋值，就具有了永久的绑定关系，后续的赋值不再改变绑定关系，而是改变被引用的变量的值。
    coroutine * m_cort;
    struct event_data * m_evd_ptr;
};

#endif // CONNECTION_H