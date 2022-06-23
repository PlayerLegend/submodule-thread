#include <time.h>
clock_t _benchmark_start_time;

#define benchmark_start() _benchmark_start_time = clock()
#define benchmark_time(msg) printf(msg ": %zu / %zu\n", clock() - _benchmark_start_time, CLOCKS_PER_SEC)
