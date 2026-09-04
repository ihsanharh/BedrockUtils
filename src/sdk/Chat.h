#pragma once
#include "intercept/PacketInterceptor.h"
#include "sdk/Factory.h"
#include "sdk/Game.h"
#include "sdk/Logger.h"
#include "sdk/packets/TextPacket.h"
#include <format>
#include <memory>
#include <string>
#include <string_view>

namespace SDK::Chat
{

// Injects an inbound raw TextPacket directly into the local client chat receiver
inline void send(std::string_view message)
{
    SDK::Log::log("[Chat] {}", message);

    if (!SDK::Game::player() || !PacketInterceptor::get().hasClientHandler())
    {
        return;
    }

    std::shared_ptr<Packet> pkt = Factory::createPacket(PacketID::TEXT);
    if (!pkt)
    {
        return;
    }

    TextPacket* tp = static_cast<TextPacket*>(pkt.get());
    tp->type = TextPacketType::RAW;
    tp->needsTranslation = false;
    tp->sourceName = message;
    tp->message = message;
    tp->xuid.clear();
    tp->platformChatId.clear();

    PacketInterceptor::get().injectInbound(pkt);
}

// Injects an inbound chat TextPacket formatted with an author name (<author> message)
inline void sendChat(std::string_view author, std::string_view message)
{
    SDK::Log::log("[Chat] <{}> {}", author, message);

    if (!SDK::Game::player() || !PacketInterceptor::get().hasClientHandler())
    {
        return;
    }

    std::shared_ptr<Packet> pkt = Factory::createPacket(PacketID::TEXT);
    if (!pkt)
    {
        return;
    }

    TextPacket* tp = static_cast<TextPacket*>(pkt.get());
    tp->type = TextPacketType::CHAT;
    tp->needsTranslation = false;
    tp->sourceName = author;
    tp->message = message;
    tp->xuid.clear();
    tp->platformChatId.clear();

    PacketInterceptor::get().injectInbound(pkt);
}

// Standard branded notification: §a§l[BedrockUtils]§r §7<message>
inline void notify(std::string_view message)
{
    send(std::format("§a§l[BedrockUtils]§r §7{}", message));
}

// Success notification: §a§l[BedrockUtils]§r §a<message>
inline void success(std::string_view message)
{
    send(std::format("§a§l[BedrockUtils]§r §a{}", message));
}

// Warning notification: §6§l[BedrockUtils]§r §e<message>
inline void warn(std::string_view message)
{
    send(std::format("§6§l[BedrockUtils]§r §e{}", message));
}

// Error notification: §c§l[BedrockUtils]§r §c<message>
inline void error(std::string_view message)
{
    send(std::format("§c§l[BedrockUtils]§r §c{}", message));
}

// Module-scoped notification: §a[<moduleName>]§r §7<message>
inline void moduleNotify(std::string_view moduleName, std::string_view message)
{
    send(std::format("§a[{}]§r §7{}", moduleName, message));
}

} // namespace SDK::Chat
