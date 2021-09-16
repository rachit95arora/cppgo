#include "Context.h"
#include "Machines.h"

#include <assert.h>

namespace go
{
    Context::Context(const ContextType contextType, Context *nextContext)
    {
        initialise(contextType, nextContext);
    }

    void Context::initialise(const ContextType contextType, Context *nextContext)
    {
        assert(contextType <= ContextType::NUM_CONTEXT_TYPES);
        getcontext(&m_ucontext);
        // setup stack in user context
        m_stack.resize(STACK_SIZES[contextType]);
        m_ucontext.uc_stack.ss_sp = m_stack.data();
        m_ucontext.uc_stack.ss_size = m_stack.size();
        m_ucontext.uc_stack.ss_flags = 0;

        // Setup signal masks
        sigemptyset(&m_ucontext.uc_sigmask);
        // only main thread will intercept timer SIGRTMIN and send SIGUSR1 to other threads
        sigaddset(&m_ucontext.uc_sigmask, SIGRTMIN);
        if (contextType == SCHEDULER)
        {
            // scheduler routines do not need to interrupt on timers
            sigaddset(&m_ucontext.uc_sigmask, SIGUSR1);
        }

        // Setup nextContext link post context termination
        m_ucontext.uc_link = (nextContext != nullptr) ? nextContext->userContext() : nullptr;

        // Make the context
        if (contextType == ContextType::SCHEDULER)
        {
            makecontext(&m_ucontext, &Machines::scheduleNext, 0);
        }
        else
        {
            makecontext(&m_ucontext, &Machines::runRoutine, 0);
        }
    }
}