#include "utils.h"



timems_t timems()
{
    timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}