#pragma once
#include <mutex>
#include <atomic>
#include <queue>

#include "Machines.h"
namespace gocpp
{

    class ChannelBase
    {
    public:
        virtual operator bool() = 0;
    };

    template <typename T>
    class ReadChannel : virtual public ChannelBase
    {
    public:
        virtual uint8_t readNoBlock(T &out) = 0;
        virtual uint8_t read(T &out) = 0;
    };

    template <typename T>
    class WriteChannel : virtual public ChannelBase
    {
    public:
        virtual uint8_t writeNoBlock(const T &in) = 0;
        virtual uint8_t write(const T &in) = 0;
        virtual void close() = 0;
    };

    template <typename T>
    class Channel : public ReadChannel<T>, public WriteChannel<T>
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
        enum ChannelRetMasks : uint8_t
        {
            FAILURE = 0b0000,
            SUCCESS = 0b0001,
            BLOCK = 0b0010
        };
        Channel(size_t buffer_size = 0)
            : m_buffer_size(buffer_size)
        {
        }
        static inline Channel EOC{};

        uint8_t readNoBlock(T &out) override
        {
            if (m_buffer_size != 0 and not m_buffered_data.empty())
            {
                SpinYieldLock<std::mutex> readLock(m_lock);
                // Let's read
                out = m_buffered_data.front();
                m_buffered_data.pop_front();
                return SUCCESS;
            }
            else if (m_state == State::READ_READY)
            {
                SpinYieldLock<std::mutex> readLock(m_lock);
                // Let's read
                out = m_data;
                m_state = State::WRITE_READY;
                return SUCCESS;
            }
            else if (m_state == State::CLOSED)
            {
                out = T{};
            }
            return FAILURE;
        }

        // read returns true if channel can still receive data
        uint8_t read(T &out) override
        {
            while (true)
            {
                if (readNoBlock(out) == SUCCESS)
                {
                    return SUCCESS;
                }
                else if (m_state == State::CLOSED)
                {
                    return FAILURE;
                }
                // Yield coro and wait
                Machines::yieldToScheduler();
            }
        }

        uint8_t writeNoBlock(const T &in) override
        {
            if (m_state == State::CLOSED)
            {
                throw std::runtime_error("Attempted write on a closed channel!");
            }
            else if (m_buffer_size != m_buffered_data.size())
            {
                SpinYieldLock<std::mutex> writeLock(m_lock);
                // Let's write
                m_buffered_data.emplace_back(in);
                return SUCCESS;
            }
            else if (m_state == State::WRITE_READY)
            {
                SpinYieldLock<std::mutex> writeLock(m_lock);
                // Let's write
                m_data = in;
                m_state = State::READ_READY;
                return SUCCESS | BLOCK;
            }
            return FAILURE;
        }

        uint8_t write(const T &in) override
        {
            while (true)
            {
                uint8_t ret = writeNoBlock(in);
                if (ret != FAILURE)
                {
                    if (ret & BLOCK)
                    {
                        while (m_state != State::WRITE_READY)
                        {
                            // Yield coro and wait
                            Machines::yieldToScheduler();
                        }
                    }
                    return SUCCESS;
                }
                // Yield coro and wait
                Machines::yieldToScheduler();
            }
        }

        void close() override
        {
            m_state = State::CLOSED;
        }

        operator bool() override
        {
            return (this != &EOC);
        }
    };

    template <typename T, template <typename> class WriteChannelType>
    auto &operator<<(WriteChannelType<T> &ch, const T &in)
    {
        ch.write(in);
        return ch;
    }

    template <typename T, template <typename> class ReadChannelType>
    auto &operator>>(ReadChannelType<T> &ch, T &out)
    {
        if (not ch.read(out))
        {
            return Channel<T>::EOC;
        }
        return ch;
    }

}