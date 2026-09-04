#pragma once
#include "pipeline/PacketContext.h"
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SDK
{

class TextPacket;
class ModalFormRequestPacket;
class AvailableCommandsPacket;

enum class CommandResponseType
{
    Text,    // Server replied with one or more text lines
    Form,    // Server replied with a ModalFormRequest popup JSON
    Timeout, // Server did not respond before timeoutMs
    Error    // PacketSender unavailable or factory failed
};

struct FormResponse
{
    uint32_t id = 0;
    std::string json;
};

struct CommandResult
{
    CommandResponseType type = CommandResponseType::Text;
    std::string command;
    std::vector<std::string> lines;
    std::optional<FormResponse> form;
    bool isSuccess = true;

    bool isForm() const { return type == CommandResponseType::Form; }
    bool isText() const { return type == CommandResponseType::Text; }
    bool isTimeout() const { return type == CommandResponseType::Timeout; }

    std::string text(std::string_view delim = "\n") const
    {
        std::string res;
        for (size_t i = 0; i < lines.size(); ++i)
        {
            if (i > 0)
            {
                res += delim;
            }
            res += lines[i];
        }
        return res;
    }
};

enum class ExpectedResponseType
{
    Any,
    Text,
    Form
};

enum class RequestLifecycle
{
    Capturing,
    Squelching // Completed/fired callback, but continuing to drop trailing/late packets within squelchMs
};

struct AvailableCommandInfo
{
    std::string name;
    std::string description;
    uint8_t flags = 0;
    uint8_t permission = 0;
};

struct CommandRequest
{
    std::string command;
    ExpectedResponseType expectedType = ExpectedResponseType::Any;
    bool silent = true;                         // hide from chat / ui
    bool waitForAvailable = true;               // wait for AvailableCommandsPacket before dispatching if not yet received
    uint32_t timeoutMs = 3000;
    uint32_t batchDebounceMs = 200;             // wait time after last line to seal multiline text
    uint32_t squelchMs = 400;                   // swallow late/trailing packet after completion
    size_t expectedLines = 0;                   // complete immediately when reaching N lines

    std::function<bool(std::string_view)> textFilter = nullptr;
    std::function<bool(uint32_t formId, std::string_view json)> formFilter = nullptr;
    std::function<void(const CommandResult&)> onComplete = nullptr;
};

class CommandManager
{
public:
    static CommandManager& get()
    {
        static CommandManager inst;
        return inst;
    }

    void init();
    bool execute(CommandRequest req);
    bool cancelPending(std::string_view command);
    void tick();
    void reset();
    void shutdown();

    // Available commands query API
    bool hasAvailableCommands() const;
    bool isCommandAvailable(std::string_view command) const;
    std::vector<AvailableCommandInfo> getAvailableCommands() const;
    std::optional<AvailableCommandInfo> getCommandInfo(std::string_view command) const;

    void setCooldownMs(uint32_t ms) { m_minCommandCooldownMs = ms; }
    uint32_t getCooldownMs() const { return m_minCommandCooldownMs; }

private:
    CommandManager() = default;
    ~CommandManager() = default;

    struct ActiveRequest
    {
        CommandRequest req;
        std::shared_ptr<Packet> packetHolder; // keep packet alive for async network thread
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastLineTime;
        std::chrono::steady_clock::time_point squelchStartTime;
        std::vector<std::string> capturedLines;
        RequestLifecycle state = RequestLifecycle::Capturing;
        bool capturingBatch = false;
    };

    mutable std::recursive_mutex m_mutex;
    std::vector<ActiveRequest> m_activeRequests;
    std::deque<CommandRequest> m_pendingQueue;
    std::chrono::steady_clock::time_point m_lastDispatchTime{};
    uint32_t m_minCommandCooldownMs = 3000; // 3s cooldown between commands as requested

    std::unordered_map<std::string, AvailableCommandInfo> m_availableCommands;
    std::unordered_set<std::string> m_knownAliases;
    bool m_hasAvailableCommands = false;
    bool m_initialized = false;

    bool dispatchInternal(CommandRequest& req);
    void handleInboundText(TypedPacketContext<TextPacket>& ctx);
    void handleInboundForm(TypedPacketContext<ModalFormRequestPacket>& ctx);
    void handleInboundAvailableCommands(TypedPacketContext<AvailableCommandsPacket>& ctx);
    void handleInboundCommandOutput(PacketContext& ctx);
};

} // namespace SDK
