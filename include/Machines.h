#pragma once
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <stdexcept>

#include "Processor.h"
#include "Executor.h"
#include "Defer.h"

#define GO_END gocpp::Machines::getInstance()->finalize();
#define BLOCKER(code) gocpp::Machines::yieldRoutinesAndProcessor();code;

namespace gocpp
{
    // Singleton class to store top level library instance information including
    // processors and executor instances
    class Machines
    {
    private:
        std::vector<RoutinePtr> m_routine_list;
        std::mutex m_routine_lock;
        std::condition_variable m_new_routine_cv;

        std::vector<ProcessorPtr> m_idleProcessors;
        std::mutex m_processors_lock;
        std::condition_variable m_idle_proc_cv;

        std::atomic_bool m_stopped{false};

        std::unordered_map<std::thread::id, ExecutorPtr> m_executors;

        timer_t m_timer_id;
        struct sigevent m_sev;
        itimerspec m_ts;
        struct sigaction m_sa;

        static inline std::thread::id s_main_thread_id;
        static inline std::atomic_uint16_t s_idle_count{0};

    private:
        Machines();
        ~Machines();

    public:
        static Machines *getInstance();
        static auto &idleCount() { return s_idle_count; }

        template <typename Fn, typename... Args>
        void submitRoutine(Fn &&fn, Args &&...args)
        {
            std::packaged_task<void(void)> task(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
            auto routinePtr = std::make_unique<Routine>(std::move(task));

            auto threadId = std::this_thread::get_id();
            if (threadId != s_main_thread_id)
            {
                auto &executorPtr = Machines::getInstance()->m_executors[threadId];
                executorPtr->processor()->submitRoutine(std::move(routinePtr));
            }
            else
            {
                std::unique_lock<std::mutex> lock(m_routine_lock);
                m_routine_list.emplace_back(std::move(routinePtr));
            }
            m_new_routine_cv.notify_one();
        }

        void pullProcessor(ProcessorPtr &processor);

        void pullRoutines(std::vector<RoutinePtr> &routines, int stealerId, bool coreIdle);

        void finalize();

        bool running() { return not m_stopped; }

        static void runRoutine();
        static void scheduleNext();

        static void yieldToScheduler();
        static void yieldRoutinesAndProcessor();
        static void sigUsrHandler(int signal);
        static void timerInterruptHandler(int sig, siginfo_t *si, void *uc);
    };
    template <typename Lock>
    class SpinYieldLock
    {
        Lock &m_lock;

    public:
        SpinYieldLock(Lock &lock)
            : m_lock(lock)
        {
            while (true)
            {
                if (m_lock.try_lock())
                {
                    return;
                }
                Machines::yieldToScheduler();
            }
        }

        ~SpinYieldLock()
        {
            m_lock.unlock();
        }
    };
}

template <typename Fn, typename... Args>
void go(Fn &&fn, Args &&...args)
{
    gocpp::Machines::getInstance()->submitRoutine(std::forward<Fn>(fn), std::forward<Args>(args)...);
}
