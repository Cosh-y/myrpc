#ifndef WORKER_H
#define WORKER_H

#include <thread>

class worker {    
public:
    worker();
    void add_client(int fd_sock);

private:
    void run();

private:
    std::thread m_thread;
    int m_id;
};

#endif
