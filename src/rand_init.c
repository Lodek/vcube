#include <time.h>
#include <stdlib.h>

#include "rand_init.h"

void rand_init() {
    struct timespec ts = {};

    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand(ts.tv_nsec);
}
