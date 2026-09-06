#include "CommandManager.h"
#include "Factory.h"
#include "Game.h"
#include "Logger.h"
#include "SafeString.h"
#include "packets/CommandRequestPacket.h"
#include "packets/ModalFormRequestPacket.h"
#include "packets/ModalFormResponsePacket.h"
#include "packets/TextPacket.h"
#include "pipeline/Pipeline.h"
#include <algorithm>
#include <new>

namespace SDK
{

static std::string extractBaseCommand(std::string_view fullCmd)
{
    while (!fullCmd.empty() && (fullCmd.front() == '/' || fullCmd.front() == ' '))
    {
        fullCmd.remove_prefix(1);
    }
    size_t spacePos = fullCmd.find(' ');
    if (spacePos != std::string_view::npos)
    {
        fullCmd = fullCmd.substr(0, spacePos);
    }
    return toLower(trim(fullCmd));
}

static bool isServerErrorOrCooldown(std::string_view cleanLineLower)
{
    return cleanLineLower.find("you're issuing commands too quickly") != std::string_view::npos
        || cleanLineLower.find("try again later") != std::string_view::npos
        || cleanLineLower.find("unknown command") != std::string_view::npos
        || cleanLineLower.find("command not found") != std::string_view::npos
        || cleanLineLower.find("does not exist") != std::string_view::npos
        || cleanLineLower.find("cannot be found") != std::string_view::npos
        || cleanLineLower.find("not recognized") != std::string_view::npos
        || cleanLineLower.find("invalid command") != std::string_view::npos
        || cleanLineLower.find("type /help") != std::string_view::npos
        || cleanLineLower.find("sorry!") != std::string_view::npos
        || cleanLineLower.find("cooldown") != std::string_view::npos
        || cleanLineLower.find("please wait") != std::string_view::npos
        || cleanLineLower.find("commands.generic.unknown") != std::string_view::npos
        || cleanLineLower.find("syntax error") != std::string_view::npos;
}

void CommandManager::init()
{
    if (m_initialized)
    {
        return;
    }
    m_initialized = true;

    // Listen for inbound TextPackets
    Pipeline::get().on<TextPacket>([this](TypedPacketContext<TextPacket>& ctx)
    {
        if (ctx.dir == PacketDirection::Inbound)
        {
            handleInboundText(ctx);
        }
    });

    // Listen for inbound ModalFormRequestPackets
    Pipeline::get().on<ModalFormRequestPacket>([this](TypedPacketContext<ModalFormRequestPacket>& ctx)
    {
        if (ctx.dir == PacketDirection::Inbound)
        {
            handleInboundForm(ctx);
        }
    });

    // Listen for inbound CommandOutputPackets (0x4F)
    Pipeline::get().on(PacketID::COMMAND_OUTPUT, [this](PacketContext& ctx)
    {
        if (ctx.dir == PacketDirection::Inbound)
        {
            handleInboundCommandOutput(ctx);
        }
    });



    // Reset all pending/active commands on world transition, transfer, or disconnect
    Pipeline::get().on(PacketID::CHANGE_DIMENSION, [this](PacketContext& ctx)
    {
        if (ctx.dir == PacketDirection::Inbound)
        {
            reset();
        }
    });

    Pipeline::get().on(PacketID::TRANSFER, [this](PacketContext& ctx)
    {
        if (ctx.dir == PacketDirection::Inbound)
        {
            reset();
        }
    });

    Pipeline::get().on(PacketID::DISCONNECT, [this](PacketContext& ctx)
    {
        if (ctx.dir == PacketDirection::Inbound)
        {
            reset();
        }
    });
}

void CommandManager::handleInboundCommandOutput(PacketContext& ctx)
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    for (const ActiveRequest& ar : m_activeRequests)
    {
        if (ar.req.silent)
        {
            ctx.drop("CommandManager: silent command output");
            break;
        }
    }
}



bool CommandManager::cancelPending(std::string_view command)
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::string base = extractBaseCommand(command);
    std::deque<CommandRequest>::iterator it = std::remove_if(m_pendingQueue.begin(), m_pendingQueue.end(), [&](const CommandRequest& r)
    {
        return extractBaseCommand(r.command) == base;
    });
    if (it != m_pendingQueue.end())
    {
        m_pendingQueue.erase(it, m_pendingQueue.end());
        return true;
    }
    return false;
}

bool CommandManager::execute(CommandRequest req)
{
    init();

    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    int64_t elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastDispatchTime).count();

    // If cooldown (3s) has elapsed and queue is empty, dispatch immediately
    if (m_pendingQueue.empty() && elapsedMs >= m_minCommandCooldownMs)
    {
        m_lastDispatchTime = now;
        return dispatchInternal(req);
    }

    // Otherwise, queue for deferred rate-limited execution
    m_pendingQueue.push_back(std::move(req));
    return true;
}

bool CommandManager::dispatchInternal(CommandRequest& req)
{
    std::function<bool(std::string_view)> fail = [&](std::string_view reason)
    {
        SDK::Log::log("[CommandManager] dispatchInternal error: {}", reason);
        if (req.onComplete)
        {
            CommandResult res;
            res.type = CommandResponseType::Error;
            res.command = req.command;
            res.isSuccess = false;
            req.onComplete(res);
        }
        return false;
    };

    PacketSender* sender = Game::sender();
    if (!sender)
    {
        return fail("PacketSender not available");
    }

    std::shared_ptr<Packet> pkt = Factory::createPacket(PacketID::COMMAND_REQUEST);
    if (!pkt)
    {
        return fail("createPacket(COMMAND_REQUEST) failed");
    }

    std::string formattedCmd = req.command;
    if (!formattedCmd.starts_with("/"))
    {
        formattedCmd = "/" + formattedCmd;
    }

    CommandRequestPacket* cr = static_cast<CommandRequestPacket*>(pkt.get());
    cr->command = formattedCmd;
    cr->origin.requestId.clear();
    cr->version = 50;
    cr->internal = false;

    bool isSilent = req.silent;
    size_t expectedLines = req.expectedLines;

    ActiveRequest ar;
    ar.req = std::move(req);
    ar.packetHolder = pkt;
    ar.startTime = std::chrono::steady_clock::now();
    ar.lastLineTime = ar.startTime;
    ar.squelchStartTime = ar.startTime;
    ar.state = RequestLifecycle::Capturing;
    ar.capturingBatch = false;
    m_activeRequests.push_back(std::move(ar));

    SDK::Log::log("[CommandManager] Dispatched command: {} (silent={}, expectedLines={})",
        formattedCmd, isSilent, expectedLines);
    sender->sendToServer(pkt.get());
    return true;
}

void CommandManager::handleInboundText(TypedPacketContext<TextPacket>& ctx)
{
    if (!ctx.packet)
    {
        return;
    }

    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        if (m_activeRequests.empty())
        {
            return;
        }
    }

    std::vector<std::string> rawLines;
    safeReadAllTextMessages(ctx.packet, rawLines);

    if (rawLines.empty())
    {
        return;
    }

    // Split raw lines containing embedded newlines so each line is processed and batched individually
    std::vector<std::string> splitLines;
    for (const std::string& raw : rawLines)
    {
        size_t start = 0;
        while (start < raw.size())
        {
            size_t end = raw.find('\n', start);
            std::string line = (end == std::string::npos)
                ? raw.substr(start)
                : raw.substr(start, end - start);

            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            if (!line.empty())
            {
                splitLines.push_back(std::move(line));
            }

            if (end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        }
    }

    if (splitLines.empty())
    {
        return;
    }

    uint8_t pktType = static_cast<uint8_t>(ctx.packet->type);
    std::vector<std::pair<std::function<void(const CommandResult&)>, CommandResult>> toFire;

    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

        for (const std::string& rawLineStr : splitLines)
        {
            std::string cleanLine = stripColorCodes(rawLineStr);
            std::string cleanLower = toLower(cleanLine);

            for (ActiveRequest& ar : m_activeRequests)
            {
                if (ar.state == RequestLifecycle::Squelching)
                {
                    if (ar.req.silent)
                    {
                        // Squelch only if:
                        // 1. Not a Form-only request (a Form request has no business squelching text!)
                        // 2. AND either textFilter matches, or it's a recognized error/cooldown line
                        if (ar.req.expectedType != ExpectedResponseType::Form)
                        {
                            bool isRelevant = false;
                            if (ar.req.textFilter)
                            {
                                isRelevant = ar.req.textFilter(rawLineStr) || ar.req.textFilter(cleanLine);
                            }
                            else
                            {
                                isRelevant = isServerErrorOrCooldown(cleanLower);
                            }

                            if (isRelevant)
                            {
                                ctx.drop("CommandManager: squelching command output");
                            }
                        }
                    }
                    continue;
                }

                bool isErrOrCooldown = isServerErrorOrCooldown(cleanLower);

                // server sent error or cooldown text
                if (isErrOrCooldown)
                {
                    if (ar.req.silent)
                    {
                        ctx.drop("CommandManager: silent command error/cooldown");
                    }

                    if (ar.capturedLines.empty())
                    {
                        if (ar.req.onComplete)
                        {
                            CommandResult res;
                            res.type = CommandResponseType::Error;
                            res.command = ar.req.command;
                            res.lines.push_back(rawLineStr);
                            res.isSuccess = false;
                            toFire.emplace_back(std::move(ar.req.onComplete), std::move(res));
                        }
                        ar.state = RequestLifecycle::Squelching;
                        ar.squelchStartTime = now;
                        continue;
                    }
                }

                // If this request is strictly waiting for a Form, do NOT capture normal text lines!
                if (ar.req.expectedType == ExpectedResponseType::Form)
                {
                    continue;
                }

                bool matches = false;
                if (ar.req.textFilter)
                {
                    matches = ar.req.textFilter(rawLineStr) || ar.req.textFilter(cleanLine);
                }
                else
                {
                    matches = (pktType != 1 && pktType != 4);
                }

                if (matches)
                {
                    ar.capturedLines.push_back(rawLineStr);
                    ar.capturingBatch = true;
                    ar.lastLineTime = now;

                    if (ar.req.silent)
                    {
                        ctx.drop("CommandManager: silent command response");
                    }

                    if (ar.req.expectedLines > 0 && ar.capturedLines.size() >= ar.req.expectedLines)
                    {
                        SDK::Log::log("[CommandManager] Completed command '{}' with all {} expected lines",
                            ar.req.command, ar.capturedLines.size());

                        if (ar.req.onComplete)
                        {
                            CommandResult res;
                            res.type = CommandResponseType::Text;
                            res.command = ar.req.command;
                            res.lines = std::move(ar.capturedLines);
                            res.isSuccess = true;
                            toFire.emplace_back(std::move(ar.req.onComplete), std::move(res));
                        }

                        ar.state = RequestLifecycle::Squelching;
                        ar.squelchStartTime = now;
                    }
                }
            }
        }
    }

    for (const std::pair<std::function<void(const CommandResult&)>, CommandResult>& item : toFire)
    {
        if (item.first)
        {
            item.first(item.second);
        }
    }
}

void CommandManager::handleInboundForm(TypedPacketContext<ModalFormRequestPacket>& ctx)
{
    if (!ctx.packet)
    {
        return;
    }

    uint32_t formId = ctx.packet->formId;
    const uint8_t* base = reinterpret_cast<const uint8_t*>(ctx.packet);

    std::string jsonStr;
    if (!safeReadString(base + 0x38, jsonStr) || jsonStr.empty())
    {
        if (!safeReadString(base + 0x34, jsonStr) || jsonStr.empty())
        {
            for (size_t off = 0x30; off <= 0x58; off += 4)
            {
                if (safeReadString(base + off, jsonStr) && jsonStr.find('{') != std::string::npos)
                {
                    break;
                }
            }
        }
    }

    if (jsonStr.empty())
    {
        return;
    }

    std::vector<std::pair<std::function<void(const CommandResult&)>, CommandResult>> toFire;

    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

        for (ActiveRequest& ar : m_activeRequests)
        {
            if (ar.state == RequestLifecycle::Squelching)
            {
                // Only squelch if this request actually had a formFilter that matches this form!
                if (ar.req.silent && ar.req.formFilter && ar.req.formFilter(formId, jsonStr))
                {
                    ctx.drop("CommandManager: squelching form request");
                }
                continue;
            }

            if (ar.req.expectedType == ExpectedResponseType::Text)
            {
                continue;
            }

            bool matches = false;
            if (ar.req.formFilter)
            {
                matches = ar.req.formFilter(formId, jsonStr);
            }
            else
            {
                matches = true;
            }

            if (matches)
            {
                if (ar.req.silent)
                {
                    ctx.drop("CommandManager: silent form request");

                    // Immediately reply with cancel (null) so server does not keep modal form open
                    if (PacketSender* sender = Game::sender())
                    {
                        if (std::shared_ptr<Packet> respPkt = Factory::createPacket(PacketID::MODAL_FORM_RESPONSE))
                        {
                            ModalFormResponsePacket* mfr = static_cast<ModalFormResponsePacket*>(respPkt.get());
                            mfr->formId = formId;
                            mfr->responseData = "null\n";
                            sender->sendToServer(respPkt.get());
                        }
                    }
                }

                if (ar.req.onComplete)
                {
                    CommandResult res;
                    res.type = CommandResponseType::Form;
                    res.command = ar.req.command;
                    res.form = FormResponse{formId, std::move(jsonStr)};
                    res.isSuccess = true;
                    toFire.emplace_back(std::move(ar.req.onComplete), std::move(res));
                }

                ar.state = RequestLifecycle::Squelching;
                ar.squelchStartTime = now;
                break;
            }
        }
    }

    for (const std::pair<std::function<void(const CommandResult&)>, CommandResult>& item : toFire)
    {
        if (item.first)
        {
            item.first(item.second);
        }
    }
}

void CommandManager::tick()
{
    std::vector<CommandResult> completed;
    std::vector<std::function<void(const CommandResult&)>> callbacks;

    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

        // 1. Process pending command queue with rate-limiting delay (3s cooldown)
        if (!m_pendingQueue.empty())
        {
            int64_t elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastDispatchTime).count();
            if (elapsedMs >= m_minCommandCooldownMs)
            {
                m_lastDispatchTime = now;
                CommandRequest nextReq = std::move(m_pendingQueue.front());
                m_pendingQueue.pop_front();
                dispatchInternal(nextReq);
            }
        }

        // 2. Process active requests timeouts, batch debounce & squelch expiration
        for (std::vector<ActiveRequest>::iterator it = m_activeRequests.begin(); it != m_activeRequests.end();)
        {
            // A. Handle Squelching state
            if (it->state == RequestLifecycle::Squelching)
            {
                int64_t squelchElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->squelchStartTime).count();
                if (squelchElapsedMs >= it->req.squelchMs)
                {
                    it = m_activeRequests.erase(it);
                }
                else
                {
                    ++it;
                }
                continue;
            }

            // B. Handle Capturing state
            int64_t elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->startTime).count();
            int64_t debounceElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->lastLineTime).count();

            bool debounceDone = it->capturingBatch && (
                debounceElapsedMs >= it->req.batchDebounceMs ||
                (it->req.expectedLines > 0 && it->capturedLines.size() >= it->req.expectedLines)
            );

            // If we were capturing lines and the debounce window closed, complete with Text
            if (debounceDone)
            {
                SDK::Log::log("[CommandManager] Batch completed command '{}' with {} lines",
                    it->req.command, it->capturedLines.size());

                CommandResult res;
                res.type = CommandResponseType::Text;
                res.command = it->req.command;
                res.lines = std::move(it->capturedLines);
                res.isSuccess = true;

                if (it->req.onComplete)
                {
                    completed.push_back(std::move(res));
                    callbacks.push_back(std::move(it->req.onComplete));
                }

                it->state = RequestLifecycle::Squelching;
                it->squelchStartTime = now;
                ++it;
            }
            // If timeout expired
            else if (elapsedMs >= it->req.timeoutMs)
            {
                SDK::Log::log("[CommandManager] Timeout for command '{}' (captured {} lines)",
                    it->req.command, it->capturedLines.size());

                CommandResult res;
                if (!it->capturedLines.empty())
                {
                    res.type = CommandResponseType::Text;
                    res.lines = std::move(it->capturedLines);
                    res.isSuccess = true;
                }
                else
                {
                    res.type = CommandResponseType::Timeout;
                    res.isSuccess = false;
                }

                res.command = it->req.command;

                if (it->req.onComplete)
                {
                    completed.push_back(std::move(res));
                    callbacks.push_back(std::move(it->req.onComplete));
                }

                it->state = RequestLifecycle::Squelching;
                it->squelchStartTime = now;
                ++it;
            }
            else
            {
                ++it;
            }
        }
    }

    // Fire callbacks outside of mutex lock to avoid deadlocks
    for (size_t i = 0; i < completed.size(); ++i)
    {
        if (callbacks[i])
        {
            callbacks[i](completed[i]);
        }
    }
}

void CommandManager::reset()
{
    std::vector<std::pair<std::function<void(const CommandResult&)>, CommandResult>> toFire;
    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        m_pendingQueue.clear();

        for (ActiveRequest& ar : m_activeRequests)
        {
            if (ar.req.onComplete)
            {
                CommandResult res;
                if (!ar.capturedLines.empty())
                {
                    res.type = CommandResponseType::Text;
                    res.lines = std::move(ar.capturedLines);
                    res.isSuccess = true;
                }
                else
                {
                    res.type = CommandResponseType::Error;
                    res.isSuccess = false;
                }
                res.command = ar.req.command;
                toFire.emplace_back(std::move(ar.req.onComplete), std::move(res));
            }
        }

        m_activeRequests.clear();
        m_lastDispatchTime = {};
    }

    for (std::pair<std::function<void(const CommandResult&)>, CommandResult>& item : toFire)
    {
        if (item.first)
        {
            item.first(item.second);
        }
    }
}

void CommandManager::shutdown()
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_pendingQueue.clear();
    m_activeRequests.clear();
    m_lastDispatchTime = {};
    m_initialized = false;
}

} // namespace SDK
