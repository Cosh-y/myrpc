#ifndef HOOK_H
#define HOOK_H

#include <unistd.h>         // POSIX 通用 IO read/write
#include <sys/socket.h>     // BSD socket API recv/send

typedef ssize_t (*original_read_t)(int, void *, size_t);
extern original_read_t original_read;

typedef ssize_t (*original_write_t)(int, const void *, size_t);
extern original_write_t original_write;

typedef ssize_t (*original_recv_t)(int, void *, size_t, int);
extern original_recv_t original_recv;

typedef ssize_t (*original_send_t)(int, const void *, size_t, int);
extern original_send_t original_send;

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
ssize_t recv(int fd, void *buf, size_t len, int flags);
ssize_t send(int fd, const void *buf, size_t len, int flags);

#endif