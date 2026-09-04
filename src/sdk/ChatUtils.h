#pragma once
#include "sdk/SafeString.h"
#include "sdk/packets/TextPacket.h"
#include <format>
#include <string>
#include <string_view>

namespace SDK
{

struct ParsedChat
{
    std::string author;
    std::string cleanMessage;
    std::string rawMessage;
};

namespace ChatUtils
{

inline std::string stripPrefixDelimiters(std::string_view text)
{
    size_t start = 0;
    while (start < text.size())
    {
        char c = text[start];
        if (c == ' ' || c == '\t' || c == ':' || c == '>' || c == '-' || c == '|' || c == '~')
        {
            start++;
            continue;
        }

        // Handle UTF-8 for '»' (0xC2 0xBB)
        if (static_cast<unsigned char>(c) == 0xC2 && start + 1 < text.size() && static_cast<unsigned char>(text[start + 1]) == 0xBB)
        {
            start += 2;
            continue;
        }

        // Handle 3-byte UTF-8 arrows and dashes ('›' \xE2\x80\xBA, '—' \xE2\x80\x94, '–' \xE2\x80\x93, '→' \xE2\x86\x92, '▶' \xE2\x96\xB6, '➜' \xE2\x9E\x9C, '➔' \xE2\x9E\x94)
        if (static_cast<unsigned char>(c) == 0xE2 && start + 2 < text.size())
        {
            unsigned char c2 = static_cast<unsigned char>(text[start + 1]);
            unsigned char c3 = static_cast<unsigned char>(text[start + 2]);
            if ((c2 == 0x80 && (c3 == 0xBA || c3 == 0x94 || c3 == 0x93)) ||
                (c2 == 0x86 && c3 == 0x92) ||
                (c2 == 0x96 && c3 == 0xB6) ||
                (c2 == 0x9E && (c3 == 0x9C || c3 == 0x94)))
            {
                start += 3;
                continue;
            }
        }

        break;
    }

    return std::string(trim(text.substr(start)));
}

inline bool hasTranslatableText(std::string_view text)
{
    for (size_t i = 0; i < text.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(text[i]);
        // ASCII letters
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        {
            return true;
        }
        // Multi-byte UTF-8 characters (CJK, Cyrillic, accented Latin, etc.)
        if (c >= 0x80)
        {
            return true;
        }
    }
    return false;
}

inline ParsedChat parse(const TextPacket* packet)
{
    ParsedChat result;
    if (!packet)
    {
        return result;
    }

    std::string sourceName;
    std::string message;
    std::string param0;
    std::string param1;

    safeReadString(&packet->sourceName, sourceName);
    safeReadString(&packet->message, message);
    safeReadVectorString(&packet->parameters, 0, param0);
    safeReadVectorString(&packet->parameters, 1, param1);

    // Handle TRANSLATION packet type (Realms, Bedrock dedicated servers, vanilla LAN)
    if (packet->type == TextPacketType::TRANSLATION)
    {
        if (!param1.empty())
        {
            result.author = std::string(trim(stripColorCodes(param0)));
            result.cleanMessage = std::string(trim(stripColorCodes(param1)));
            result.rawMessage = std::format("<{}> {}", param0, param1);
            return result;
        }
        else if (!param0.empty())
        {
            result.cleanMessage = std::string(trim(stripColorCodes(param0)));
            result.rawMessage = param0;
            return result;
        }
    }

    // Handle standard CHAT packet type
    if (packet->type == TextPacketType::CHAT && !sourceName.empty())
    {
        std::string msgStr = message.empty() ? packet->getMessage() : message;
        std::string clean = stripColorCodes(msgStr);

        result.author = std::string(trim(stripColorCodes(sourceName)));
        result.rawMessage = std::format("<{}> {}", sourceName, msgStr);

        // Check if msgStr contains embedded author delimiters (e.g. "Author » msg" or "Author: msg" or "Author > msg")
        if (size_t delimPos = clean.find("\xC2\xBB"); delimPos != std::string::npos && delimPos + 2 < clean.size())
        {
            result.cleanMessage = std::string(stripPrefixDelimiters(clean.substr(delimPos + 2)));
        }
        else if (size_t colonPos = clean.find(':'); colonPos != std::string::npos && colonPos + 1 < clean.size())
        {
            result.cleanMessage = std::string(stripPrefixDelimiters(clean.substr(colonPos + 1)));
        }
        else if (size_t delimPos = clean.find('>'); delimPos != std::string::npos && delimPos + 1 < clean.size())
        {
            result.cleanMessage = std::string(stripPrefixDelimiters(clean.substr(delimPos + 1)));
        }
        else
        {
            result.cleanMessage = std::string(trim(clean));
        }
        return result;
    }

    // For RAW, SYSTEM_MESSAGE, and custom server packet types
    std::string rawAuthor = sourceName;
    std::string rawMsg = message;

    if (rawMsg.empty() && !param0.empty())
    {
        rawMsg = param0;
    }

    if (rawMsg.empty())
    {
        rawMsg = packet->getMessage();
    }

    std::string cleanAuthor = stripColorCodes(rawAuthor);
    std::string cleanMsg = stripColorCodes(rawMsg);

    std::string parsedAuthor = cleanAuthor;
    std::string parsedMsg = cleanMsg;

    // 1. Check for <Author> Message format
    if (parsedMsg.starts_with("<"))
    {
        size_t closeAngle = parsedMsg.find('>');
        if (closeAngle != std::string::npos && closeAngle + 1 < parsedMsg.size())
        {
            if (parsedAuthor.empty())
            {
                parsedAuthor = parsedMsg.substr(1, closeAngle - 1);
            }
            parsedMsg = stripPrefixDelimiters(parsedMsg.substr(closeAngle + 1));
        }
    }
    // 2. Check for [Rank] Author: Message or Author: Message
    else if (size_t colonPos = parsedMsg.find(':'); colonPos != std::string::npos && colonPos + 1 < parsedMsg.size())
    {
        std::string possibleAuthor = parsedMsg.substr(0, colonPos);
        std::string afterColon = stripPrefixDelimiters(parsedMsg.substr(colonPos + 1));

        if (!afterColon.empty())
        {
            if (parsedAuthor.empty())
            {
                parsedAuthor = cleanPlayerName(possibleAuthor);
            }
            parsedMsg = afterColon;
        }
    }
    // 3. Check for Author » Message or [Guild] Author » Message
    else if (size_t delimPos = parsedMsg.find("\xC2\xBB"); delimPos != std::string::npos && delimPos + 2 < parsedMsg.size())
    {
        std::string possibleAuthor = parsedMsg.substr(0, delimPos);
        std::string afterDelim = stripPrefixDelimiters(parsedMsg.substr(delimPos + 2));

        if (!afterDelim.empty())
        {
            if (parsedAuthor.empty())
            {
                parsedAuthor = cleanPlayerName(possibleAuthor);
            }
            parsedMsg = afterDelim;
        }
    }
    // 4. Check for Author > Message
    else if (size_t delimPos = parsedMsg.find('>'); delimPos != std::string::npos && delimPos + 1 < parsedMsg.size())
    {
        std::string possibleAuthor = parsedMsg.substr(0, delimPos);
        std::string afterDelim = stripPrefixDelimiters(parsedMsg.substr(delimPos + 1));

        if (!afterDelim.empty())
        {
            if (parsedAuthor.empty())
            {
                parsedAuthor = cleanPlayerName(possibleAuthor);
            }
            parsedMsg = afterDelim;
        }
    }

    parsedAuthor = std::string(trim(cleanPlayerName(parsedAuthor)));
    parsedMsg = std::string(trim(parsedMsg));

    result.author = parsedAuthor;
    result.cleanMessage = parsedMsg;
    result.rawMessage = rawMsg;
    return result;
}

} // namespace ChatUtils
} // namespace SDK
