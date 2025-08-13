#include "scheduler.h"

int main() {
    provider prov;
    scheduler sched(prov);
    sched.run();
}