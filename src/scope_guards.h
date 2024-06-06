#pragma once

#include <algorithm>

namespace scope_guards
{
// ScopeGuard for C++11, Andrei Alexandrescu
// https://skydrive.live.com/view.aspx?resid=F1B8FF18A2AEC5C5!1158&app=WordPdf&authkey=!APo6bfP5sJ8EmH4
template <typename FType>
class OnScopeExit
{
    FType cleanup;
    std::atomic<bool> is_active;

public:
    explicit OnScopeExit(FType cleanup_function)
        : cleanup(std::move(cleanup_function))
        , is_active(true)
    {
    }

    ~OnScopeExit()
    {
        if (is_active)
            cleanup();
    }

    OnScopeExit()                              = delete;
    OnScopeExit(const OnScopeExit&)            = delete;
    OnScopeExit& operator=(const OnScopeExit&) = delete;

    OnScopeExit(OnScopeExit&& other)
        : cleanup(std::move(other.cleanup))
        , is_active(other.exchange(false))
    {
    }

    void dismiss() { is_active = false; }
};

template <typename FType>
auto on_scope_exit_(FType cleanup_function)
{
    return OnScopeExit<FType>(std::move(cleanup_function));
}

#ifndef _VAR_CAT3
#define _VAR_CAT3(a, b, c) a##b##c
#endif
#ifndef VAR_CAT3
#define VAR_CAT3(a, b, c) _VAR_CAT3(a, b, c)
#endif
#define on_scope_exit(cleanup_function) \
    volatile auto VAR_CAT3(__PRETTY_FUNC__, _cleanup_, __LINE__) = scope_guards::on_scope_exit_(cleanup_function);

} // namespace scope_guards