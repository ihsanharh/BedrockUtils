#include "pch.h"
#include "SigScan.h"
#include <vector>

namespace SigScan
{

struct Byte
{ 
    uint8_t val;
    bool wild;
};

static std::vector<Byte> parse(std::string_view pattern)
{
    std::vector<Byte> out;

    for (size_t i = 0; i < pattern.size(); )
    {
        if (pattern[i] == ' ') 
        {
            ++i;
            continue;
        }

        if (pattern[i] == '?')
        {
            out.push_back({0, true});

            if (i + 1 < pattern.size() && pattern[i + 1] == '?')
            {
                i += 2;
            }
            else
            {
                i += 1;
            }
            
            continue;
        }

        uint8_t (*h)(char) = [](char c) -> uint8_t
        {
            if (c >= '0' && c <= '9')
            {
                return static_cast<uint8_t>(c - '0');
            }
            if (c >= 'A' && c <= 'F')
            {
                return static_cast<uint8_t>(c - 'A' + 10);
            }
            if (c >= 'a' && c <= 'f')
            {
                return static_cast<uint8_t>(c - 'a' + 10);
            }
            return 0;
        };
        
        uint8_t hi = h(pattern[i]);
        uint8_t lo = (i + 1 < pattern.size() && pattern[i + 1] != ' ') ? h(pattern[i + 1]) : 0;
        out.push_back({static_cast<uint8_t>((hi << 4) | lo), false});
        i += (i + 1 < pattern.size() && pattern[i + 1] != ' ') ? 2 : 1;
    }

    return out;
}

uintptr_t scan(uintptr_t base, size_t size, std::string_view pattern)
{
    const std::vector<Byte> pat = parse(pattern);
    
    if (pat.empty() || size < pat.size())
    {
        return 0;
    }
    
    const uint8_t* mem = reinterpret_cast<const uint8_t*>(base);
    
    for (size_t i = 0, lim = size - pat.size(); i <= lim; ++i)
    {
        bool ok = true;

        for (size_t j = 0; j < pat.size(); ++j)
        {
            if (!pat[j].wild && mem[i + j] != pat[j].val)
            {
                ok = false;
                break;
            }
        }
        
        if (ok)
        {
            return base + i;
        }
    }

    return 0;
}

uintptr_t find(const char* moduleName, std::string_view pattern)
{
    HMODULE hMod = GetModuleHandleA(moduleName);

    if (!hMod)
    {
        return 0;
    }
    
    const IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(hMod);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return 0;
    }

    const IMAGE_NT_HEADERS* nt  = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uintptr_t>(hMod) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
    {
        return 0;
    }

    return scan(reinterpret_cast<uintptr_t>(hMod), nt->OptionalHeader.SizeOfImage, pattern);
}

uintptr_t resolveRel32(uintptr_t instrAddr, int relOffset, int instrSize)
{
    int32_t rel = *reinterpret_cast<const int32_t*>(instrAddr + relOffset);
    return static_cast<uintptr_t>(static_cast<intptr_t>(instrAddr) + instrSize + rel);
}

} // namespace SigScan
