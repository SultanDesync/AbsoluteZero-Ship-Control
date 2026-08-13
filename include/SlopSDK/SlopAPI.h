#pragma once

// Stable C ABI for the Starfield Local Options Panel. Providers keep ownership of
// their settings and callbacks; SLOP copies descriptors during registration and
// never passes a C++ object across a DLL boundary.

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace SlopApi
{
    inline constexpr std::uint32_t kAbiVersion = 1;
    inline constexpr std::size_t kIdentifierCapacity = 64;
    inline constexpr std::size_t kLabelCapacity = 96;
    inline constexpr std::size_t kDescriptionCapacity = 192;
    inline constexpr std::size_t kStringValueCapacity = 256;

    enum class Result : std::uint32_t
    {
        Ok,
        NotReady,
        InvalidArgument,
        Duplicate,
        NotFound,
        CapacityExceeded,
        Rejected,
        WriteFailure
    };

    enum class ControlKind : std::uint32_t
    {
        Toggle,
        IntegerSlider,
        FloatSlider,
        Choice,
        Action,
        ButtonBinding
    };

    enum class ValueKind : std::uint32_t
    {
        Boolean,
        Integer,
        Float,
        String
    };

    enum ControlFlags : std::uint32_t
    {
        kControlNone = 0,
        kControlReadOnly = 1U << 0,
        kControlRequiresRestart = 1U << 1,
        kControlAdvanced = 1U << 2
    };

    struct ValueV1
    {
        std::uint32_t structSize{ sizeof(ValueV1) };
        ValueKind kind{ ValueKind::String };
        std::uint32_t booleanValue{};
        std::int64_t integerValue{};
        double floatValue{};
        char stringValue[kStringValueCapacity]{};
    };

    struct ControlDescriptorV1
    {
        std::uint32_t structSize{ sizeof(ControlDescriptorV1) };
        ControlKind kind{ ControlKind::Toggle };
        std::uint32_t flags{ kControlNone };
        char controlId[kIdentifierCapacity]{};
        char label[kLabelCapacity]{};
        char description[kDescriptionCapacity]{};
        double minimumValue{};
        double maximumValue{};
        double stepValue{};
    };

    using ReadValueCallback = Result(__cdecl*)(
        void* context, const char* controlId, ValueV1* value) noexcept;
    using WriteDraftCallback = Result(__cdecl*)(
        void* context, const char* controlId, const ValueV1* value) noexcept;
    using InvokeActionCallback = Result(__cdecl*)(
        void* context, const char* controlId) noexcept;
    using ApplyCallback = Result(__cdecl*)(void* context) noexcept;
    using CancelCallback = void(__cdecl*)(void* context) noexcept;

    struct ModuleDescriptorV1
    {
        std::uint32_t structSize{ sizeof(ModuleDescriptorV1) };
        char moduleId[kIdentifierCapacity]{};
        char displayName[kLabelCapacity]{};
        char description[kDescriptionCapacity]{};
    };

    struct PageDescriptorV1
    {
        std::uint32_t structSize{ sizeof(PageDescriptorV1) };
        char moduleId[kIdentifierCapacity]{};
        char pageId[kIdentifierCapacity]{};
        char displayName[kLabelCapacity]{};
        char description[kDescriptionCapacity]{};
        std::uint32_t controlCount{};
        const ControlDescriptorV1* controls{};
        void* context{};
        ReadValueCallback readValue{};
        WriteDraftCallback writeDraft{};
        InvokeActionCallback invokeAction{};
        ApplyCallback apply{};
        CancelCallback cancel{};
    };

    struct ApiV1
    {
        std::uint32_t structSize{ sizeof(ApiV1) };
        std::uint32_t abiVersion{ kAbiVersion };
        const char* moduleId{};
        const char* displayName{};
        const char* version{};

        Result(__cdecl* registerPage)(const PageDescriptorV1*) noexcept{};
        Result(__cdecl* unregisterModule)(const char* moduleId) noexcept{};
        Result(__cdecl* requestRefresh)(
            const char* moduleId, const char* pageId) noexcept{};
        Result(__cdecl* registerModule)(const ModuleDescriptorV1*) noexcept{};
    };

    static_assert(std::is_standard_layout_v<ValueV1>);
    static_assert(std::is_trivially_copyable_v<ValueV1>);
    static_assert(std::is_standard_layout_v<ControlDescriptorV1>);
    static_assert(std::is_trivially_copyable_v<ControlDescriptorV1>);
    static_assert(std::is_standard_layout_v<ModuleDescriptorV1>);
    static_assert(std::is_trivially_copyable_v<ModuleDescriptorV1>);
    static_assert(std::is_standard_layout_v<PageDescriptorV1>);
    static_assert(std::is_standard_layout_v<ApiV1>);
}

#if defined(SLOP_EXPORTS)
#define SLOP_API __declspec(dllexport)
#else
#define SLOP_API __declspec(dllimport)
#endif

extern "C" SLOP_API const SlopApi::ApiV1*
SLOP_QueryApi(std::uint32_t requestedAbiVersion) noexcept;
