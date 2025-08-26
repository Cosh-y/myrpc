#include <iostream>
#include <thread>
#include <vector>

#include "scheduler.h"

int main() {
    int n_threads = std::thread::hardware_concurrency();
    n_threads = 2;
    std::cout << "std::thread::hardware_concurrency: " << n_threads << "\n";
    std::vector<std::thread> thread_pool;

    for (int i = 0;i < n_threads;i++) {
        thread_pool.emplace_back([]() {
            scheduler sched;
            sched.run();
        });
    }

    for (auto & t : thread_pool) {
        t.join();
    }
}