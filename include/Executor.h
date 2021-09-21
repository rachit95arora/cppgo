#pragma once

#include "Processor.h"
#include "Context.h"

namespace gocpp
{

    class Executor;
    using ExecutorPtr = std::unique_ptr<Executor>;

    class Executor
    {
        ProcessorPtr m_processor{};
        RoutinePtr m_active_routine{};
        std::thread m_thread;
        ContextPtr m_scheduler_context;
        std::atomic_bool m_running{false};
        int m_id{-1};

    private:
    public:
        Executor(int id);
        ~Executor();

        auto threadId() const
        {
            return m_thread.get_id();
        }

        auto &thread()
        {
            return m_thread;
        }

        auto id() const { return m_id; }

        void finalize() { m_running = false; }

        void yieldProcessor(ProcessorPtr& proc); 

        void scheduleLoop();

        void switchToScheduler();

        const ProcessorPtr &processor() { return m_processor; }

        void runActiveRoutine();
    };

} // end namespace gocpp