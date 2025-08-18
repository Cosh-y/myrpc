#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>

class connection {
public:
    connection() = default;
    connection(int fd_sock);
    void run();
    int id() { return m_id; }
    
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
};

#endif // CONNECTION_H