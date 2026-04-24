#include "TimeUtils.h"
#include <chrono>

uint64_t TimeUtils::getCurrentTime()
{
    using namespace std::chrono;

    return duration_cast<nanoseconds>(
        high_resolution_clock::now().time_since_epoch()
    ).count();
}