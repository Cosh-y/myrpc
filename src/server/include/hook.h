#ifndef HOOK_H
#define HOOK_H

#include "scheduler.h"

typedef ssize_t (*original_read_t)(int, void *, size_t);
extern original_read_t original_read;
typedef ssize_t (*original_write_t)(int, const void *, size_t);
extern original_write_t original_write;

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

#endif