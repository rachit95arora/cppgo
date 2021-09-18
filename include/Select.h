#pragma once

#include "Channel.h"

#include <type_traits>

namespace gocpp
{
    namespace detail
    {
        struct SelectCase
        {
            virtual ~SelectCase() = default;
            virtual SelectCase *copy() = 0;
            virtual bool operator()() const = 0;
        };

        template <typename T>
        struct ReadCase : public SelectCase
        {
            ReadChannel<T> *const m_chan{nullptr};
            T *const m_obj{nullptr};

            ReadCase(ReadChannel<T> *chan, T *obj)
                : m_chan(chan), m_obj(obj)
            {
            }

            SelectCase *copy() override
            {
                return new ReadCase(*this);
            }

            bool operator()() const override
            {
                return m_chan->readReady() and m_chan->read(*m_obj);
            }
        };

        template <typename T>
        struct WriteCase : public SelectCase
        {
            WriteChannel<T> *const m_chan{nullptr};
            const T *const m_obj{nullptr};

            WriteCase(WriteChannel<T> *chan, const T *obj)
                : m_chan(chan), m_obj(obj)
            {
            }

            SelectCase *copy() override
            {
                return new WriteCase(*this);
            }

            bool operator()() const override
            {
                return m_chan->writeReady() and m_chan->write(*m_obj);
            }
        };

        struct DefaultCase : public SelectCase
        {
            DefaultCase() = default;
            SelectCase *copy() override
            {
                return new DefaultCase(*this);
            }

            bool operator()() const override
            {
                // Never called!
                assert(false);
                return false;
            }
        };
    }

    class Select
    {
    public:
        struct CaseDescriptor
        {
            std::unique_ptr<detail::SelectCase> m_condition;
            std::function<void()> m_callable;
            bool m_default{false};

            template <typename CaseCondition>
            CaseDescriptor(CaseCondition &&condition, std::function<void()> &&fn)
                : m_condition(new CaseCondition(condition)), m_callable(std::move(fn)), m_default(std::is_same_v<CaseCondition, detail::DefaultCase>)
            {
            }
            CaseDescriptor() = default;

            CaseDescriptor(const CaseDescriptor &other)
                : m_condition(other.m_condition->copy()), m_callable(other.m_callable), m_default(other.m_default)
            {
            }

            CaseDescriptor &operator=(const CaseDescriptor &other)
            {
                m_condition.reset(other.m_condition->copy());
                m_callable = other.m_callable;
                m_default = other.m_default;
                return *this;
            }
        };

    public:
        Select(std::initializer_list<CaseDescriptor> list)
        {
            for (auto &desc : list)
            {
                if (desc.m_default)
                {
                    if (m_defaultCase.m_default)
                    {
                        throw std::runtime_error("Only one default case should be specified!");
                    }
                    m_defaultCase = desc;
                }
                else
                {
                    m_cases.emplace_back(desc);
                }
            }
        }

        void operator()()
        {
            while (true)
            {
                for (auto &desc : m_cases)
                {
                    if ((*desc.m_condition)())
                    {
                        desc.m_callable();
                        return;
                    }
                }

                if (m_defaultCase.m_default)
                {
                    m_defaultCase.m_callable();
                    return;
                }
            }
        }

    private:
        std::vector<CaseDescriptor> m_cases;
        CaseDescriptor m_defaultCase{};
    };

    using Case = Select::CaseDescriptor;
    inline Case DefaultCase(std::function<void()> callable) { return Case(gocpp::detail::DefaultCase{}, std::move(callable)); }

    template <typename T, template <typename> class WriteChannelType>
    auto operator>=(const T &in, WriteChannelType<T> &ch)
    {
        return detail::WriteCase<T>(&ch, &in);
    }

    template <typename T, template <typename> class ReadChannelType>
    auto operator<=(T &out, ReadChannelType<T> &ch)
    {
        return detail::ReadCase<T>(&ch, &out);
    }

    template <typename T, template <typename> class WriteChannelType>
    auto operator<=(WriteChannelType<T> &ch, const T &in)
    {
        return detail::WriteCase<T>(&ch, &in);
    }

    template <typename T, template <typename> class ReadChannelType>
    auto operator>=(ReadChannelType<T> &ch, T &out)
    {
        return detail::ReadCase<T>(&ch, &out);
    }
}
