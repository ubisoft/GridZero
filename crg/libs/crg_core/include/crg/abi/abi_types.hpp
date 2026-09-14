// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// abi_types.hpp — ABI-safe trivially-copyable types for plugin boundaries

#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>

namespace crg::abi {

    struct StringView {
        const char* m_Data{nullptr};
        uint64_t    m_Length{0};

        StringView() = default;
        StringView(const char* data, uint64_t length) noexcept
            : m_Data(data), m_Length(length) {}
        StringView(std::string_view sv) noexcept
            : m_Data(sv.data()), m_Length(static_cast<uint64_t>(sv.size())) {}

        operator std::string_view() const noexcept {
            return {m_Data, static_cast<size_t>(m_Length)};
        }
        const char* data() const noexcept   { return m_Data; }
        uint64_t    size() const noexcept   { return m_Length; }
        bool        empty() const noexcept  { return m_Length == 0; }
    };
    static_assert(std::is_trivially_copyable_v<StringView>);
    static_assert(std::is_standard_layout_v<StringView>);

    template<typename T>
    struct Span {
        const T* m_Data{nullptr};
        uint64_t m_Count{0};

        Span() = default;
        Span(const T* data, uint64_t count) noexcept
            : m_Data(data), m_Count(count) {}

        const T* begin() const noexcept { return m_Data; }
        const T* end()   const noexcept { return m_Data + m_Count; }
        uint64_t size()  const noexcept { return m_Count; }
        bool     empty() const noexcept { return m_Count == 0; }
        const T& operator[](uint64_t i) const noexcept { return m_Data[i]; }
    };
    static_assert(std::is_trivially_copyable_v<Span<StringView>>);
    static_assert(std::is_standard_layout_v<Span<StringView>>);

    using LogCallbackFunc =
        void(*)(const char* textData, uint64_t textLength, void* userData) noexcept;

    struct Logger {
        LogCallbackFunc m_Callback{nullptr};
        void*           m_UserData{nullptr};

        void Log(StringView text) const noexcept {
            if (m_Callback) m_Callback(text.m_Data, text.m_Length, m_UserData);
        }
        void Log(std::string_view text) const noexcept {
            if (m_Callback)
                m_Callback(text.data(),
                           static_cast<uint64_t>(text.size()),
                           m_UserData);
        }
        bool Active() const noexcept { return m_Callback != nullptr; }
    };
    static_assert(std::is_trivially_copyable_v<Logger>);
    static_assert(std::is_standard_layout_v<Logger>);

} // namespace crg::abi
