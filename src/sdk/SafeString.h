#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>

namespace SDK
{

template<typename T>
struct PODVector
{
    T* _Myfirst = nullptr;
    T* _Mylast  = nullptr;
    T* _Myend   = nullptr;

    [[nodiscard]] size_t size() const noexcept
    {
        return (_Myfirst && _Mylast && _Mylast >= _Myfirst) ? static_cast<size_t>(_Mylast - _Myfirst) : 0;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return size() == 0;
    }

    void clear() noexcept
    {
        _Myfirst = _Mylast = _Myend = nullptr;
    }

    void push_back(const T& val)
    {
        if (_Mylast == _Myend)
        {
            size_t oldCap = _Myend ? static_cast<size_t>(_Myend - _Myfirst) : 0;
            size_t newCap = oldCap == 0 ? 8 : oldCap * 2;
            size_t curSize = size();
            T* newBuf = static_cast<T*>(::operator new(newCap * sizeof(T)));
            if (_Myfirst && curSize > 0)
            {
                std::memcpy(newBuf, _Myfirst, curSize * sizeof(T));
                ::operator delete(_Myfirst);
            }
            _Myfirst = newBuf;
            _Mylast = newBuf + curSize;
            _Myend = newBuf + newCap;
        }
        *_Mylast = val;
        ++_Mylast;
    }

    [[nodiscard]] const T* data() const noexcept
    {
        return _Myfirst;
    }

    [[nodiscard]] T* data() noexcept
    {
        return _Myfirst;
    }

    [[nodiscard]] const T& operator[](size_t idx) const noexcept
    {
        return _Myfirst[idx];
    }

    [[nodiscard]] T& operator[](size_t idx) noexcept
    {
        return _Myfirst[idx];
    }

    [[nodiscard]] const T* begin() const noexcept
    {
        return _Myfirst;
    }

    [[nodiscard]] const T* end() const noexcept
    {
        return _Mylast;
    }

    [[nodiscard]] T* begin() noexcept
    {
        return _Myfirst;
    }

    [[nodiscard]] T* end() noexcept
    {
        return _Mylast;
    }
};
static_assert(sizeof(PODVector<void*>) == 24, "PODVector size mismatch with MSVC std::vector");

struct SafeString
{
    union
    {
        char        buf[16];
        const char* ptr;
    };
    size_t size = 0;
    size_t res = 15;

    SafeString()
    {
        buf[0] = '\0';
        size = 0;
        res = 15;
    }

    SafeString(std::string_view sv)
    {
        assign(sv);
    }

    SafeString(const char* s)
        : SafeString(s ? std::string_view(s) : std::string_view{}) {}

    SafeString(const std::string& s)
        : SafeString(std::string_view(s)) {}

    SafeString(const SafeString& o)
        : SafeString(o.view()) {}

    SafeString(SafeString&& o) noexcept
    {
        if (o.res < 16)
        {
            std::memcpy(buf, o.buf, 16);
        }
        else
        {
            ptr = o.ptr;
        }
        size = o.size;
        res = o.res;
        o.buf[0] = '\0';
        o.size = 0;
        o.res = 15;
    }

    // Trivially destructible: never free foreign heap pointers
    ~SafeString() = default;

    void clear()
    {
        buf[0] = '\0';
        size = 0;
        res = 15;
    }

    void assign(std::string_view sv)
    {
        if (sv.size() < 16)
        {
            if (!sv.empty())
            {
                std::memcpy(buf, sv.data(), sv.size());
            }
            buf[sv.size()] = '\0';
            size = sv.size();
            res = 15;
        }
        else
        {
            size_t newCap = (sv.size() | 0x0F);
            char* heapBuf = static_cast<char*>(std::malloc(newCap + 1));
            if (heapBuf)
            {
                std::memcpy(heapBuf, sv.data(), sv.size());
                heapBuf[sv.size()] = '\0';
                ptr = heapBuf;
                size = sv.size();
                res = newCap;
            }
            else
            {
                std::memcpy(buf, sv.data(), 15);
                buf[15] = '\0';
                size = 15;
                res = 15;
            }
        }
    }

    SafeString& operator=(SafeString&& o) noexcept
    {
        if (this != &o)
        {
            if (o.res < 16)
            {
                std::memcpy(buf, o.buf, 16);
            }
            else
            {
                ptr = o.ptr;
            }
            size = o.size;
            res = o.res;
            o.buf[0] = '\0';
            o.size = 0;
            o.res = 15;
        }
        return *this;
    }

    SafeString& operator=(const SafeString& o)
    {
        if (this != &o)
        {
            assign(o.view());
        }
        return *this;
    }

    SafeString& operator=(std::string_view sv)
    {
        assign(sv);
        return *this;
    }

    SafeString& operator=(const char* s)
    {
        if (s)
        {
            assign(std::string_view(s));
        }
        else
        {
            clear();
        }
        return *this;
    }

    SafeString& operator=(const std::string& s)
    {
        assign(std::string_view(s));
        return *this;
    }

    std::string str() const;
    std::string_view view() const;

    operator std::string() const
    {
        return str();
    }

    operator std::string_view() const noexcept
    {
        return view();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return view().empty();
    }

    [[nodiscard]] size_t length() const noexcept
    {
        return view().size();
    }

    [[nodiscard]] const char* c_str() const noexcept
    {
        return view().data();
    }

    [[nodiscard]] bool operator==(std::string_view other) const noexcept
    {
        return view() == other;
    }

    [[nodiscard]] bool operator==(const SafeString& other) const noexcept
    {
        return view() == other.view();
    }

    [[nodiscard]] bool operator==(const std::string& other) const noexcept
    {
        return view() == other;
    }

    [[nodiscard]] bool operator==(const char* other) const noexcept
    {
        return other ? view() == other : empty();
    }

    friend std::ostream& operator<<(std::ostream& os, const SafeString& s);
};

static_assert(sizeof(SafeString) == 32, "SafeString size mismatch with MSVC std::string");

inline bool safeReadString(const void* strAddr, std::string& out)
{
    __try
    {
        if (!strAddr)
        {
            return false;
        }

        const SafeString* s = reinterpret_cast<const SafeString*>(strAddr);
        if (s->res < 16)
        {
            if (s->size == 0)
            {
                out.clear();
                return true;
            }
            if (s->size <= 15)
            {
                out.assign(s->buf, s->size);
                return true;
            }
            return false;
        }
        else
        {
            if (s->size > 0 && s->size <= 131072 && s->ptr)
            {
                out.assign(s->ptr, s->size);
                return true;
            }
            return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

inline std::string SafeString::str() const
{
    std::string out;
    safeReadString(this, out);
    return out;
}

inline std::string_view SafeString::view() const
{
    __try
    {
        if (res < 16)
        {
            if (size <= 15)
            {
                return std::string_view(buf, size);
            }
            return {};
        }
        else
        {
            if (size > 0 && size <= 131072 && ptr)
            {
                return std::string_view(ptr, size);
            }
            return {};
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return {};
    }
}

inline std::ostream& operator<<(std::ostream& os, const SafeString& s)
{
    return os << s.view();
}

inline bool safeReadVectorString(const void* vecAddr, size_t index, std::string& out)
{
    __try
    {
        if (!vecAddr)
        {
            return false;
        }
        const uintptr_t* vec = reinterpret_cast<const uintptr_t*>(vecAddr);
        uintptr_t first = vec[0];
        uintptr_t last  = vec[1];
        if (!first || !last || last <= first)
        {
            return false;
        }
        size_t count = (last - first) / 32; // sizeof(std::string) in MSVC x64 is 32 bytes
        if (index >= count || count > 100)
        {
            return false;
        }
        const void* strAddr = reinterpret_cast<const void*>(first + index * 32);
        return safeReadString(strAddr, out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// Safely reads all text messages in a TextPacket (message, sourceName, parameters[0..9], and scanned offsets)
inline void safeReadAllTextMessages(const void* packetBase, std::vector<std::string>& outList)
{
    if (!packetBase)
    {
        return;
    }
    const uint8_t* base = reinterpret_cast<const uint8_t*>(packetBase);

    std::string s;
    if (safeReadString(base + 0xC8, s) && !s.empty())
    {
        outList.push_back(s);
    }

    if (safeReadString(base + 0xA8, s) && !s.empty())
    {
        if (std::find(outList.begin(), outList.end(), s) == outList.end())
        {
            outList.push_back(s);
        }
    }

    for (size_t i = 0; i < 10; ++i)
    {
        if (safeReadVectorString(base + 0x78, i, s) && !s.empty())
        {
            if (std::find(outList.begin(), outList.end(), s) == outList.end())
            {
                outList.push_back(s);
            }
        }
    }

    if (outList.empty())
    {
        for (size_t off = 0x30; off <= 0x120; off += 8)
        {
            if (safeReadString(base + off, s) && !s.empty())
            {
                if (std::find(outList.begin(), outList.end(), s) == outList.end())
                {
                    outList.push_back(s);
                }
            }
        }
    }
}

inline bool safeReadTextMessage(const void* packetBase, std::string& out)
{
    std::vector<std::string> list;
    safeReadAllTextMessages(packetBase, list);
    if (!list.empty())
    {
        out = list.front();
        return true;
    }
    return false;
}

// Trims whitespace from both ends of a string_view
inline std::string_view trim(std::string_view sv)
{
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\r' || sv.front() == '\n' || sv.front() == '\t'))
    {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\r' || sv.back() == '\n' || sv.back() == '\t'))
    {
        sv.remove_suffix(1);
    }
    return sv;
}

// Strips Minecraft formatting and color codes (e.g. §c, \xC2\xA7a)
inline std::string stripColorCodes(std::string_view input)
{
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] == '\xC2' && i + 2 < input.size() && input[i + 1] == '\xA7')
        {
            i += 2;
            continue;
        }
        if (input[i] == '\xA7' && i + 1 < input.size())
        {
            i += 1;
            continue;
        }
        result += input[i];
    }
    return result;
}

// Returns a lowercased copy of the input
inline std::string toLower(std::string_view input)
{
    std::string result;
    result.reserve(input.size());
    for (char c : input)
    {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

// Returns an uppercased copy of the input
inline std::string toUpper(std::string_view input)
{
    std::string result;
    result.reserve(input.size());
    for (char c : input)
    {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return result;
}

// Cleans a raw player name or nametag by stripping color codes, newlines, and bracketed tags ([Rank], [Guild])
inline std::string cleanPlayerName(std::string_view input)
{
    std::string s = stripColorCodes(input);
    size_t nl = s.find_first_of("\r\n");
    if (nl != std::string::npos)
    {
        s.resize(nl);
    }
    std::string_view sv = trim(s);

    if (sv.starts_with('['))
    {
        size_t close = sv.find(']');
        if (close != std::string::npos)
        {
            sv = trim(sv.substr(close + 1));
        }
    }
    size_t open = sv.find('[');
    if (open != std::string::npos)
    {
        sv = trim(sv.substr(0, open));
    }

    return std::string(sv);
}

// Formats a 16-byte UUID into standard 8-4-4-4-12 hex string
inline std::string uuidToString(const uint8_t uuid[16])
{
    return std::format("{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
}

} // namespace SDK

template<>
struct std::formatter<SDK::SafeString> : std::formatter<std::string_view>
{
    std::format_context::iterator format(const SDK::SafeString& s, std::format_context& ctx) const
    {
        return std::formatter<std::string_view>::format(s.view(), ctx);
    }
};
