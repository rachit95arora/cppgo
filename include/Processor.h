#pragma once
#include <queue>
#include <mutex>

#include "Routine.h"
namespace gocpp
{
    /*
    Class that encapsulates all the coroutines switching on each core/thread (executor).
    An executor needs to acquire a processor to get routine work to execute from.
    */
    class Processor
    {
        // Lock to avoid access race to processor routines
        std::mutex m_lock;
        // Round robin queue for routines to run on the owning executor
        std::deque<RoutinePtr> m_routines;
        // Processor id
        int m_id{-1};

        // Helper function to pull from global routine queue or steal from other processors
        bool pullMoreRoutines(bool coreIdle);

    public:
        Processor(int id)
            : m_id(id)
        {
        }
        auto id() const { return m_id; }

        // Adds the routine to the routine queue to be executed on this core
        void submitRoutine(RoutinePtr &&routine);

        // Returns true if more routines are present in queue, or if some routines were fetched
        // Will block if core is idle until some routine is available
        bool hasRoutines(bool coreIdle = true);

        // Allows other cores to steal half of this processor's routines in a concurrent safe manner
        bool surrenderRoutines(std::vector<RoutinePtr> &stolenRoutines, bool all = false);

        // Returns a next routine to run on the core, swaps out current routine (if present) to the back of the queue
        // to be resumed later
        void nextRoutine(RoutinePtr &currentRoutine, RoutinePtr &next);
    };
    using ProcessorPtr = std::unique_ptr<Processor>;
}