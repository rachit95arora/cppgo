#include "Machines.h"
#include <iostream>
#include <chrono>
namespace gocpp
{
    Machines::Machines()
        : m_routine_list(), m_idleProcessors(MAX_PROCS), m_executors(MAX_PROCS)
    {
        m_stopped = false;
        s_idle_count = 0;
        s_main_thread_id = std::this_thread::get_id();

        for (int i = 0; i < MAX_PROCS; i++)
        {
            m_idleProcessors[i] = std::make_unique<Processor>(i);
            auto executor = std::make_unique<Executor>(i);
            m_executors[executor->threadId()] = std::move(executor);
        }
        // register timer
        m_sa.sa_flags = SA_SIGINFO;
        m_sa.sa_sigaction = timerInterruptHandler;
        sigemptyset(&m_sa.sa_mask);
        if (sigaction(SIGRTMIN, &m_sa, NULL) == -1)
        {
            assert(false);
        }

        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGRTMIN);
        if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
        {
            assert(false);
        }

        m_sev.sigev_notify = SIGEV_SIGNAL;
        m_sev.sigev_signo = SIGRTMIN;
        m_sev.sigev_value.sival_ptr = &m_timer_id;
        if (timer_create(CLOCK_REALTIME, &m_sev, &m_timer_id) == -1)
        {
            assert(false);
        }

        // Start the timer
        m_ts.it_interval.tv_sec = 0;
        m_ts.it_interval.tv_nsec = TIMER_NANOS;
        m_ts.it_value.tv_sec = 0;
        m_ts.it_value.tv_nsec = TIMER_NANOS;

        if (timer_settime(m_timer_id, 0, &m_ts, NULL) == -1)
        {
            assert(false);
        }

        if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
        {
            assert(false);
        }
    }

    Machines::~Machines()
    {
        finalize();
    }

    Machines *Machines::getInstance()
    {
        static Machines lInfo{};
        return &lInfo;
    }

    void Machines::pullProcessor(ProcessorPtr &processor)
    {
        if (processor)
        {
            return;
        }

        auto const getProcessor = [&]()
        {
            processor.swap(m_idleProcessors.back());
            m_idleProcessors.pop_back();
        };
        while (running())
        {
            std::unique_lock<std::mutex> lock(m_processors_lock);
            if (m_idleProcessors.empty())
            {
                // sleep now
                using namespace std::chrono_literals;
                m_idle_proc_cv.wait_for(lock, 10ms, [&]()
                                        { return not running() or not m_idleProcessors.empty(); });
            }
            else
            {
                getProcessor();
                return;
            }
        }
    }

    void Machines::pullRoutines(std::vector<RoutinePtr> &routines, int stealerId, bool coreIdle)
    {
        using namespace std::chrono_literals;
        while (running())
        {
            // Try stealing first
            for (auto &[tid, executor] : m_executors)
            {
                if (executor->processor() and executor->processor()->id() != stealerId and executor->processor()->surrenderRoutines(routines))
                {
                    return;
                }
            }

            std::unique_lock<std::mutex> routineLock(m_routine_lock);
            if (not m_routine_list.empty())
            {
                while (not m_routine_list.empty())
                {
                    routines.emplace_back(std::move(m_routine_list.back()));
                    m_routine_list.pop_back();
                }
                return;
            }

            if (not coreIdle)
            {
                // no time to waste
                return;
            }
            m_new_routine_cv.wait_for(routineLock, 100ms);
        }
    }

    void Machines::runRoutine()
    {
        auto threadId = std::this_thread::get_id();
        if (threadId != s_main_thread_id)
        {
            auto &executorPtr = Machines::getInstance()->m_executors[threadId];
            executorPtr->runActiveRoutine();
        }
        else
        {
            assert(false);
        }
    }

    void Machines::scheduleNext()
    {
        auto threadId = std::this_thread::get_id();
        if (threadId != s_main_thread_id)
        {
            auto &executorPtr = Machines::getInstance()->m_executors[threadId];
            executorPtr->scheduleLoop();
        }
        else
        {
            assert(false);
        }
    }

    void Machines::yieldToScheduler()
    {
        auto threadId = std::this_thread::get_id();
        if (threadId != s_main_thread_id)
        {
            auto &executorPtr = Machines::getInstance()->m_executors[threadId];
            executorPtr->switchToScheduler();
        }
        else
        {
            // do nothing
        }
    }

    void Machines::yieldRoutinesAndProcessor()
    {
        auto threadId = std::this_thread::get_id();
        auto* machine = Machines::getInstance();
        if (threadId != s_main_thread_id)
        {
            auto &executorPtr = machine->m_executors[threadId];
            ProcessorPtr proc;
            executorPtr->yieldProcessor(proc);
            if (proc)
            {
                std::unique_lock<std::mutex> routineLock(machine->m_routine_lock);
                proc->surrenderRoutines(machine->m_routine_list, true);
                std::unique_lock<std::mutex> procLock(machine->m_processors_lock);
                machine->m_idleProcessors.emplace_back(std::move(proc));
            }
        }
        else
        {
            assert(false);
        }
    }

    void Machines::sigUsrHandler(int signal)
    {
        yieldToScheduler();
    }

    void Machines::timerInterruptHandler(int sig, siginfo_t *si, void *uc)
    {
        for (auto &[tid, executorPtr] : Machines::getInstance()->m_executors)
        {
            pthread_kill(executorPtr->thread().native_handle(), SIGUSR1);
        }
    }

    void Machines::finalize()
    {
        using namespace std::chrono_literals;

        size_t seenIdle = 0;
        while (seenIdle < 20)
        {
            if (s_idle_count == m_executors.size())
            {
                seenIdle++;
            }
            else
            {
                seenIdle = 0;
            }
            std::this_thread::sleep_for(100ms);
        }
        m_stopped = true;
        for (auto &[tid, exec] : m_executors)
        {
            exec->finalize();
            exec->thread().join();
        }
    }
}