#pragma once
#include <mutex>
#include <atomic>
#include <queue>

#include "Machines.h"
namespace go
{
    template <typename T>
    class Channel
    {
    private:
        enum class State : uint8_t
        {
            WRITE_READY,
            READ_READY,
            CLOSED
        };
        std::mutex m_lock;
        T m_data;
        const size_t m_buffer_size{0};
        std::deque<T> m_buffered_data;
        std::atomic<State> m_state{State::WRITE_READY};

    public:
        Channel(size_t buffer_size = 0)
            : m_buffer_size(buffer_size)
        {
        }
        static inline Channel EOC{};

        // read returns true if channel can still receive data
        bool read(T &out)
        {
            while (true)
            {
                if (m_buffer_size != 0)
                {
                    if (not m_buffered_data.empty())
                    {
                        SpinYieldLock<std::mutex> readLock(m_lock);
                        // Let's read
                        out = m_buffered_data.front();
                        m_buffered_data.pop_front();
                        return true;
                    }
                }
                if (m_state == State::READ_READY)
                {
                    SpinYieldLock<std::mutex> readLock(m_lock);
                    // Let's read
                    out = m_data;
                    m_state = State::WRITE_READY;
                    return true;
                }
                else if (m_state == State::CLOSED)
                {
                    out = T{};
                    return false;
                }
                else
                {
                    // Yield coro and wait
                    Machines::yieldToScheduler();
                }
            }
        }

        void write(const T &in)
        {
            if (m_state == State::CLOSED)
            {
                throw std::runtime_error("Attempted write on a closed channel!");
            }
            while (true)
            {
                if (m_buffer_size != m_buffered_data.size())
                {
                    SpinYieldLock<std::mutex> writeLock(m_lock);
                    // Let's write
                    m_buffered_data.emplace_back(in);
                    return;
                }
                if (m_state == State::WRITE_READY)
                {
                    {
                        SpinYieldLock<std::mutex> writeLock(m_lock);
                        // Let's write
                        m_data = in;
                        m_state = State::READ_READY;
                    }
                    while (m_state != State::WRITE_READY)
                    {
                        // Yield coro and wait
                        Machines::yieldToScheduler();
                    }
                    return;
                }
                else
                {
                    // Yield coro and wait
                    Machines::yieldToScheduler();
                }
            }
        }

        void close()
        {
            m_state = State::CLOSED;
        }

        Channel &operator<<(const T &in)
        {
            write(in);
            return *this;
        }

        Channel &operator>>(T &out)
        {
            if (not read(out))
            {
                return EOC;
            }
            return *this;
        }

        operator bool()
        {
            return (this != &EOC);
        }
    };

}