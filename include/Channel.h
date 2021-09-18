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
        virtual bool readReady() = 0;
        virtual bool read(T &out) = 0;
    };

    template <typename T>
    class WriteChannel : virtual public ChannelBase
    {
    public:
        virtual bool writeReady() = 0;
        virtual bool write(const T &in) = 0;
        virtual void close() = 0;
    };

    template <typename T>
    class Channel : public ReadChannel<T>, public WriteChannel<T>
    {
    private:
        // This state will be used to maintain read/write status across concurrent channel usage
        enum State : uint8_t
        {
            DEFAULT = 0b0000,
            WRITER_BLOCKING = 0b0001,
            READER_BLOCKING = 0b0010,
            WRITE_COMPLETE = 0b0100,
            CLOSED = 0b1000
        };

        std::mutex m_lock;
        T m_data;
        const size_t m_buffer_size{0};
        std::deque<T> m_buffered_data;
        std::atomic<uint8_t> m_state{State::DEFAULT};

    private:
        bool writeComplete()
        {
            return m_state & State::WRITE_COMPLETE;
        }

        bool closed()
        {
            return m_state & State::CLOSED;
        }

        void set(State st = State::DEFAULT)
        {
            m_state |= st;
        }
        void unset(State st)
        {
            m_state &= ~st;
        }

        bool readNoBlock(T &out)
        {
            if (not m_buffered_data.empty())
            {
                SpinYieldLock<std::mutex> readLock(m_lock);
                // Let's read
                out = m_buffered_data.front();
                m_buffered_data.pop_front();
                return true;
            }
            else if (writeComplete())
            {
                SpinYieldLock<std::mutex> readLock(m_lock);
                // Let's read
                out = m_data;
                unset(State::WRITE_COMPLETE);
                return true;
            }
            else if (closed())
            {
                out = T{};
            }
            return false;
        }

        uint8_t writeNoBlock(const T &in)
        {
            if (closed())
            {
                throw std::runtime_error("Attempted write on a closed channel!");
            }
            else if (m_buffer_size != m_buffered_data.size())
            {
                SpinYieldLock<std::mutex> writeLock(m_lock);
                // Let's write
                m_buffered_data.emplace_back(in);
                return true;
            }
            else if (!writeComplete())
            {
                SpinYieldLock<std::mutex> writeLock(m_lock);
                // Let's write
                m_data = in;
                set(State::WRITE_COMPLETE);
                return true;
            }
            return false;
        }

    public:
        Channel(size_t buffer_size = 0)
            : m_buffer_size(buffer_size)
        {
        }

        Channel(const Channel &other) = delete;
        Channel(Channel &&other) = delete;

        static inline Channel EOC{};

        bool readReady() override
        {
            return m_state & State::WRITER_BLOCKING;
        }

        bool writeReady() override
        {
            return m_state & State::READER_BLOCKING;
        }

        // read returns true if channel can still receive data
        bool read(T &out) override
        {
            set(State::READER_BLOCKING);
            while (true)
            {
                if (readNoBlock(out))
                {
                    unset(State::READER_BLOCKING);
                    return true;
                }
                else if (closed())
                {
                    unset(State::READER_BLOCKING);
                    return false;
                }
                // Yield coro and wait
                Machines::yieldToScheduler();
            }
        }

        bool write(const T &in) override
        {
            set(State::WRITER_BLOCKING);
            while (true)
            {
                if (writeNoBlock(in))
                {
                    while (writeComplete())
                    {
                        // Yield coro and wait
                        Machines::yieldToScheduler();
                    }
                    unset(State::WRITER_BLOCKING);
                    return true;
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