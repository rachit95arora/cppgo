#include <memory>

#include "Routine.h"
#include "Machines.h"
#include "Context.h"

namespace gocpp
{
    Routine::Routine(std::packaged_task<void(void)> &&task)
    {
        m_fn = std::move(task); // moves the task
        m_done = false;

        m_scheduler_context = std::make_unique<Context>(ContextType::SCHEDULER);
        m_context = std::make_unique<Context>(ContextType::ROUTINE, m_scheduler_context.get());
    }

    void Routine::run()
    {
        m_fn();
        m_done = true;
    }

}