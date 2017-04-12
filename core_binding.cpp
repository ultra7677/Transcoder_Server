#include "transcoder_core.h"

core_t cores;

void init_core_t()
{
    pthread_mutex_init(&cores.core_mutex, NULL);
	cores.core_cnt = sysconf(_SC_NPROCESSORS_CONF);
    cores.next_core_id = 0;
}

void bind_core()
{
	cpu_set_t mask;
	CPU_ZERO(&mask);

    pthread_mutex_lock(&cores.core_mutex);
	CPU_SET(cores.next_core_id % cores.core_cnt, &mask);
    cores.next_core_id++;
    pthread_mutex_unlock(&cores.core_mutex);

	if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0)
        printf("bind core failed\n");
}
