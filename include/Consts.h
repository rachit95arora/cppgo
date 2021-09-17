#pragma once
#include <stdint.h>
#include <thread>
#include <pthread.h>

namespace gocpp
{
// Constants and macros relevant for the go runtime

enum ContextType : size_t
{
    ROUTINE = 0,
    SCHEDULER = 1,
    NUM_CONTEXT_TYPES = 2
};

#define TID (std::hash<std::thread::id>()(std::this_thread::get_id()) & size_t(0xFF))

#ifndef GOMAXPROCS
    static const size_t MAX_PROCS = std::thread::hardware_concurrency();
#else
    static const size_t MAX_PROCS = std::min(std::thread::hardware_concurrency(), std::max(1, GOMAXPROCS));
#endif

#ifndef GOSTACKSIZE
    static const size_t ROUTINE_STACK_SIZE = 4 * 1024 * 1024; // 4 MB
#else
    static const size_t ROUTINE_STACK_SIZE = std::max(1024 * 64, GOSTACKSIZE); // Atleast 64 KB
#endif
    static const size_t SCHED_STACK_SIZE = 64 * 1024; // 64 KB
    static const size_t STACK_SIZES[] = { ROUTINE_STACK_SIZE, SCHED_STACK_SIZE, 0};
    static const size_t TIMER_NANOS = 20'000'000; // 20ms

}