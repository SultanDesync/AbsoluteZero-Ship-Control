#pragma once
#include <string_view>
#include <REL/Relocation.h>

namespace Plugin
{
    using namespace std::string_view_literals;

    static constexpr auto Name{ "AbsoluteZero"sv };
    static constexpr auto Author{ "Antigravity"sv };
    static constexpr auto Version{
        REL::Version{ 1, 3, 0, 0 }
    };
}
