#define _GNU_SOURCE

#include "affinity.h"

#include <stdio.h>

#ifdef __linux__
#include <sched.h>
#endif

int pin_current_to_core(int core_id)
{
    if (core_id < 0) {
        fprintf(stderr, "warning: invalid affinity core id %d\n", core_id);
        return -1;
    }
#ifdef __linux__
    {
        cpu_set_t cpuset;
        if (core_id >= CPU_SETSIZE) {
            fprintf(stderr,
                    "warning: affinity core id %d exceeds CPU_SETSIZE\n",
                    core_id);
            return -1;
        }
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);
        if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
            fprintf(stderr,
                    "warning: sched_setaffinity failed for core %d\n",
                    core_id);
            return -1;
        }
        return 0;
    }
#else
    fprintf(stderr,
            "warning: CPU affinity requested but this platform is not Linux\n");
    (void)core_id;
    return -1;
#endif
}
