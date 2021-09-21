#include "Executor.h"
#include "Machines.h"
#include <iostream>

namespace gocpp
{

    Executor::Executor(int id)
        : m_id(id)
    {
        m_running = true;
        m_thread = std::thread([this]()
                               {
                                   signal(SIGUSR1, &Machines::sigUsrHandler);
                                   switchToScheduler();
                               });
    }

    Executor::~Executor()
    {
        m_thread.join();
    }

    void Executor::scheduleLoop()
    {
        Machines::idleCount()++;
        while (m_running)
        {
            if (m_processor)
            {
                // std::cerr << TID << " Found processor\n";
                if (m_active_routine and m_active_routine->done())
                {
                    // std::cerr << TID << " Routine done\n";
                    // We might be in this routine's scheduler context
                    // Keep it around to avoid any segmentation faults
                    m_scheduler_context.swap(m_active_routine->schedulerContext());
                    m_active_routine.reset();
                }
                RoutinePtr nextRoutine;
                m_processor->nextRoutine(m_active_routine, nextRoutine);
                if (nextRoutine)
                {
                    // std::cerr << TID << " Found next routine\n";
                    m_active_routine.swap(nextRoutine);
                    Machines::idleCount()--;
                    setcontext(m_active_routine->runContext()->userContext());
                }
                else if (m_active_routine)
                {
                    // just complete this routine's execution
                    // std::cerr << TID << " Continuing last routine\n";
                    Machines::idleCount()--;
                    setcontext(m_active_routine->runContext()->userContext());
                }
            }
            else
            {
                Machines::getInstance()->pullProcessor(m_processor);
            }
        }
        Machines::idleCount()--;
    }

    void Executor::switchToScheduler()
    {
        if (m_scheduler_context)
        {
            m_scheduler_context->initialise(ContextType::SCHEDULER);
        }
        else
        {
            m_scheduler_context = std::make_unique<Context>(ContextType::SCHEDULER);
        }

        if (m_active_routine)
        {
            swapcontext(m_active_routine->runContext()->userContext(), m_scheduler_context->userContext());
            // std::cerr << "Returned from scheduler!\n";
        }
        else
        {
            setcontext(m_scheduler_context->userContext());
        }
    }

    void Executor::yieldProcessor(ProcessorPtr& proc)
    {
        proc.swap(m_processor); 
        m_processor.reset();
    }

    void Executor::runActiveRoutine()
    {
        if (m_active_routine)
        {
            m_active_routine->run();
        }
        else
        {
            assert(false);
        }
    }

}