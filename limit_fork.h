/* Adapted from CPE 357 Lab 7 Given */

#ifndef LIMIT_FORK
#define LIMIT_FORK

#include <sys/resource.h>

void limit_fork(rlim_t max_procs);

#endif

