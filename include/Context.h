#pragma once
#include "Consts.h"

#include <vector>
#include <ucontext.h>
namespace go
{
    /*
    Encapsulates the user space context (registers and stack) associated with a given coroutine.
    Also contains the stack memory vector for the coroutine
    Supports two types of contexts:
    * ROUTINE: This has a larger stack and unmasked SIGUSR to allow for preemption. Starts with run function
    * SCHEDULER: This has a more limited stack size since it mainly just loops over the routine queues and masked SIGUSR since
      preemption is unnecessary. Starts the schedule function
    */
    class Context
    {
        ucontext_t m_ucontext;
        std::vector<char> m_stack;

    public:
        Context(const ContextType contextType, Context *nextContext = nullptr);
        // Sets up the stack, signal masks, entrypoint and next context
        void initialise(const ContextType contextType, Context *nextContext = nullptr);

        // accessors
        ucontext_t *userContext() { return &m_ucontext; }
    };
    using ContextPtr = std::unique_ptr<Context>;
}