#pragma once
#include <atomic>
#include <cstdint>

class IdGenerator {
public:
    static uint64_t generateId();

private:
    static std::atomic<uint64_t> currentId;
};