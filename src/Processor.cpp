#include "Processor.h"
#include "Machines.h"
#include <iostream>
namespace gocpp
{

    bool Processor::surrenderRoutines(std::vector<RoutinePtr> &stolenRoutines, bool all)
    {
        std::unique_lock<std::mutex> lock(m_lock);
        size_t routinesToLeave = all ? 0 : (1 + m_routines.size()) >> 1;
        size_t routinesToSurrender = m_routines.size() - routinesToLeave;
        while (m_routines.size() > routinesToLeave)
        {
            stolenRoutines.emplace_back(std::move(m_routines.back()));
            m_routines.pop_back();
        }
        return routinesToLeave > 0;
    }

    bool Processor::pullMoreRoutines(bool coreIdle)
    {
        std::vector<RoutinePtr> moreRoutines;
        // The machine should help processor with some global or stolen routines
        Machines::getInstance()->pullRoutines(moreRoutines, id(), coreIdle);
        // Lock to access the routine dequeue
        std::unique_lock<std::mutex> lock(m_lock);
        bool routinesAdded = not moreRoutines.empty();
        for (auto &routine : moreRoutines)
        {
            m_routines.emplace_back(std::move(routine));
        }
        return routinesAdded;
    }

    bool Processor::hasRoutines(bool coreIdle)
    {
        std::unique_lock<std::mutex> lock(m_lock);
        if (not m_routines.empty())
        {
            return true;
        }
        lock.unlock();
        return pullMoreRoutines(coreIdle);
    }

    void Processor::nextRoutine(RoutinePtr &currentRoutine, RoutinePtr &next)
    {
        next.reset();
        bool notIdle = currentRoutine and not currentRoutine->done();
        if (!hasRoutines(not notIdle))
        {
            return;
        }
        std::unique_lock<std::mutex> lock(m_lock);
        if (not m_routines.empty())
        {
            next.swap(m_routines.front());
            m_routines.pop_front();
            if (notIdle)
            {
                // keep the current routine in the back of work list
                m_routines.emplace_back(std::move(currentRoutine));
            }
        }
    }

    void Processor::submitRoutine(RoutinePtr &&routinePtr)
    {
        std::unique_lock<std::mutex> lock(m_lock);
        m_routines.emplace_back(std::move(routinePtr));
    }
}