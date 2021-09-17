#pragma once

#include "Consts.h"

#define defer(code) auto _UID(_defer_object_) = createScopeGuard([&]() { code; })
namespace gocpp
{

    template <typename Fn>
    class ScopeGuard
    {
        Fn m_fn;

    public:
        ScopeGuard(Fn &&fn)
            : m_fn(fn)
        {
        }

        ~ScopeGuard()
        {
            m_fn();
        }
    };

    template <typename Fn>
    auto createScopeGuard(Fn &&fn)
    {
        return ScopeGuard<Fn>(std::move(fn));
    }

}