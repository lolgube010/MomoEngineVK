#pragma once
#include <deque>
#include <functional>
#include <ranges>

struct DeletionQueue
{
    std::deque<std::function<void()>> _deleters;

    void Push_Function(std::function<void()>&& aFunction) { _deleters.push_back(std::move(aFunction)); }

    void Flush()
    {
        for (auto& deleter : std::ranges::reverse_view(_deleters))
        {
            deleter();
        }
        _deleters.clear();
    }
};
