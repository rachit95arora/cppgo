#pragma once
#include <functional>
#include <future>
#include <memory>

#include "Consts.h"
#include "Context.h"

namespace go
{
    /*
    This class encapsulates the [Go]Routine which is essentially a task function submitted by the user
    and the relevant stacks and context to suspend and resume it from.
    */
    class Routine
    {
        // Callable representing the routine, called via m_context
        std::packaged_task<void(void)> m_fn;
        // Context for the routine to suspend/resume from and scheduler context to ultimately exit to
        ContextPtr m_context, m_scheduler_context;
        // Boolean to indicate coroutine completion
        bool m_done{false};

    public:
        Routine(std::packaged_task<void(void)> &&task);
        // No copying/moving a routine once it is created
        // Copying it around can have unwanted user side effects
        // Moving is enabled by the wrapping ptr object
        Routine(const Routine &) = delete;
        Routine(Routine &&) = delete;
        Routine &operator=(const Routine &) = delete;
        Routine &operator=(Routine &&) = delete;

        // Calls the underlying callable and sets done on completion
        void run();

        // accessors
        bool done() const { return m_done; }
        ContextPtr& runContext() { return m_context; }
        ContextPtr& schedulerContext() { return m_scheduler_context; }

    };
    using RoutinePtr = std::unique_ptr<Routine>;
}