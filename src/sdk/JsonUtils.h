#pragma once
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace SDK::Json
{

// Decodes standard JSON escape sequences (\", \\, \/, \b, \f, \n, \r, \t)
// and handles \uXXXX unicode escapes, including Minecraft section sign (\u00A7 -> §)
// and multi-byte UTF-8 character sequences.
inline std::string unescapeJsonString(std::string_view str)
{
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '\\' && i + 1 < str.size())
        {
            char next = str[i + 1];
            if (next == '"' || next == '\\' || next == '/')
            {
                result.push_back(next);
                i++;
            }
            else if (next == 'b')
            {
                result.push_back('\b');
                i++;
            }
            else if (next == 'f')
            {
                result.push_back('\f');
                i++;
            }
            else if (next == 'n')
            {
                result.push_back('\n');
                i++;
            }
            else if (next == 'r')
            {
                result.push_back('\r');
                i++;
            }
            else if (next == 't')
            {
                result.push_back('\t');
                i++;
            }
            else if (next == 'u' && i + 5 < str.size())
            {
                std::string hexStr(str.substr(i + 2, 4));
                try
                {
                    unsigned long code = std::stoul(hexStr, nullptr, 16);
                    if (code == 0x00A7 || code == 0x00a7)
                    {
                        result.push_back('\xC2');
                        result.push_back('\xA7');
                    }
                    else if (code < 0x80)
                    {
                        result.push_back(static_cast<char>(code));
                    }
                    else if (code < 0x800)
                    {
                        result.push_back(static_cast<char>(0xC0 | (code >> 6)));
                        result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    }
                    else
                    {
                        result.push_back(static_cast<char>(0xE0 | (code >> 12)));
                        result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    }
                }
                catch (...) {}
                i += 5;
            }
            else
            {
                result.push_back(next);
                i++;
            }
        }
        else
        {
            result.push_back(str[i]);
        }
    }

    return result;
}

// Escapes special characters for standard JSON payload serialization
inline std::string escapeJson(std::string_view str)
{
    std::string res;
    res.reserve(str.size() * 2);

    for (char c : str)
    {
        switch (c)
        {
        case '"':  res += "\\\""; break;
        case '\\': res += "\\\\"; break;
        case '\b': res += "\\b";  break;
        case '\f': res += "\\f";  break;
        case '\n': res += "\\n";  break;
        case '\r': res += "\\r";  break;
        case '\t': res += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                res += std::format("\\u{:04x}", static_cast<unsigned int>(static_cast<unsigned char>(c)));
            }
            else
            {
                res += c;
            }
            break;
        }
    }

    return res;
}

// Extracts and unescapes a string property by key from a JSON object
inline std::string extractJsonString(std::string_view json, std::string_view key)
{
    std::string pattern = std::format("\"{}\"", key);
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string_view::npos)
    {
        return "";
    }

    size_t colonPos = json.find(':', keyPos + pattern.length());
    if (colonPos == std::string_view::npos)
    {
        return "";
    }

    size_t valStart = json.find_first_not_of(" \t\r\n", colonPos + 1);
    if (valStart == std::string_view::npos || json[valStart] != '"')
    {
        return "";
    }

    valStart += 1;
    size_t valEnd = valStart;
    while (valEnd < json.length())
    {
        if (json[valEnd] == '"' && json[valEnd - 1] != '\\')
        {
            break;
        }
        valEnd++;
    }

    if (valEnd > json.length() || valEnd == valStart)
    {
        return "";
    }

    std::string_view rawVal = json.substr(valStart, valEnd - valStart);
    return unescapeJsonString(rawVal);
}

// Extracts button texts from standard Bedrock ModalForm JSON payloads
inline std::vector<std::string> extractFormButtons(std::string_view json)
{
    std::vector<std::string> buttons;
    size_t buttonsPos = json.find("\"buttons\"");
    if (buttonsPos == std::string_view::npos)
    {
        return buttons;
    }

    size_t arrayStart = json.find('[', buttonsPos);
    if (arrayStart == std::string_view::npos)
    {
        return buttons;
    }

    size_t i = arrayStart + 1;
    while (i < json.size())
    {
        if (json[i] == ']')
        {
            break;
        }
        if (json[i] == '{')
        {
            size_t objStart = i;
            int depth = 1;
            i++;
            while (i < json.size() && depth > 0)
            {
                if (json[i] == '{')
                {
                    depth++;
                }
                else if (json[i] == '}')
                {
                    depth--;
                }
                else if (json[i] == '"')
                {
                    i++;
                    while (i < json.size() && json[i] != '"')
                    {
                        if (json[i] == '\\' && i + 1 < json.size())
                        {
                            i++;
                        }
                        i++;
                    }
                }
                i++;
            }

            std::string_view objStr = json.substr(objStart, i - objStart);
            size_t textKey = objStr.find("\"text\"");
            if (textKey != std::string_view::npos)
            {
                size_t colon = objStr.find(':', textKey);
                if (colon != std::string_view::npos)
                {
                    size_t valStart = objStr.find('"', colon);
                    if (valStart != std::string_view::npos)
                    {
                        size_t valEnd = valStart + 1;
                        while (valEnd < objStr.size() && objStr[valEnd] != '"')
                        {
                            if (objStr[valEnd] == '\\' && valEnd + 1 < objStr.size())
                            {
                                valEnd++;
                            }
                            valEnd++;
                        }
                        if (valEnd < objStr.size())
                        {
                            std::string_view rawVal = objStr.substr(valStart + 1, valEnd - (valStart + 1));
                            buttons.push_back(unescapeJsonString(rawVal));
                        }
                    }
                }
            }
        }
        else
        {
            i++;
        }
    }
    return buttons;
}

} // namespace SDK::Json
