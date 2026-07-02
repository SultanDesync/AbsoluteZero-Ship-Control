#pragma once

/* +++++++++++++++++++++++++ C++23 Standard Library +++++++++++++++++++++++++ */

// C standard library headers
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <cinttypes>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// C++ standard library headers
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <compare>
#include <concepts>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <version>

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

// Removed RE/Starfield.h to prevent global IDDatabase initializations
#include <SFSE/Impl/PCH.h>
#include <REL/REL.h>
#include <SFSE/SFSE.h>

#include "Plugin.h"

using namespace std::literals;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
