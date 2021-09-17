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
            virtual bool operator()(bool block) const = 0;
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

            bool operator()(bool block) const override
            {
                if (block)
                {
                    return m_chan->read(*m_obj);
                }
                else
                {
                    return m_chan->readNoBlock(*m_obj);
                }
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

            bool operator()(bool block) const override
            {
                if (block)
                {
                    return m_chan->write(*m_obj);
                }
                else
                {
                    return m_chan->writeNoBlock(*m_obj);
                }
            }
        };

        struct DefaultCase : public SelectCase
        {
            bool operator()(bool) const override
            {
                return false;
            }
        };
    }

    static inline detail::DefaultCase defaultCase{};
    class SelectEvaluator
    {
    public:
        struct CaseDescriptor
        {
            detail::SelectCase *m_condition{nullptr};
            std::function<void()> m_callable;
            bool m_default{false};

            CaseDescriptor(const CaseDescriptor &other) = default;

            template <typename CaseCondition>
            CaseDescriptor(CaseCondition &&condition, std::function<void()> &&fn)
                : m_condition(new CaseCondition(std::move(condition))), m_callable(std::move(fn)), m_default(std::is_same_v<CaseCondition, detail::DefaultCase>)
            {
            }
        };

    public:
        SelectEvaluator(std::initializer_list<CaseDescriptor> list)
        {
            for (auto &desc : list)
            {
                if (desc.m_default)
                {
                    if (m_defaultCasePtr)
                    {
                        throw std::runtime_error("Only one default case should be specified!");
                    }
                    m_defaultCasePtr = std::make_unique<CaseDescriptor>(desc);
                }
                else
                {
                    m_cases.emplace_back(desc);
                }
            }
        }

        void operator()()
        {
            bool block = not m_defaultCasePtr;
            while (true)
            {
                for (auto &desc : m_cases)
                {
                    if ((*desc.m_condition)(block))
                    {
                        desc.m_callable();
                        return;
                    }
                }

                if (block)
                {
                    Machines::yieldToScheduler();
                }
                else
                {
                    m_defaultCasePtr->m_callable();
                    return;
                }
            }
        }

    private:
        std::vector<CaseDescriptor> m_cases;
        std::unique_ptr<CaseDescriptor> m_defaultCasePtr;
    };

    using CASE = SelectEvaluator::CaseDescriptor;

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
