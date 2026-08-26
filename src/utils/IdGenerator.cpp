#include "utils/IdGenerator.h"

std::atomic<uint64_t> IdGenerator::currentId{1};

uint64_t IdGenerator::generateId()
{
    return currentId.fetch_add(1, std::memory_order_relaxed);
}