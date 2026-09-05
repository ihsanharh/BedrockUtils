#pragma once
#include <chrono>
#include <format>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <windows.h>

namespace SDK::Log
{

inline bool tryOpenLogStream(std::ofstream& stream, std::string_view filename, std::string& outResolvedPath)
{
    std::vector<std::string> candidates;

    char localAppData[MAX_PATH] = {};
    if (GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH) > 0)
    {
        // 1. Direct AppData Local
        candidates.push_back(std::string(localAppData) + "\\BedrockUtils\\logs");
        // 2. Minecraft UWP AppContainer LocalState (always writable by Minecraft)
        candidates.push_back(std::string(localAppData) + "\\Packages\\Microsoft.MinecraftUWP_8wekyb3d8bbwe\\LocalState\\BedrockUtils\\logs");
        candidates.push_back(std::string(localAppData) + "\\Packages\\Microsoft.MinecraftWindows_8wekyb3d8bbwe\\LocalState\\BedrockUtils\\logs");
    }

    // 3. Public folder fallback (always writable by AppContainer)
    candidates.push_back("C:\\Users\\Public\\BedrockUtils\\logs");
    candidates.push_back("C:\\Users\\Public");

    for (const std::string& dir : candidates)
    {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        std::string fullPath = dir + "\\" + std::string(filename);
        stream.open(fullPath, std::ios::out | std::ios::trunc);
        if (stream.is_open() && stream.good())
        {
            outResolvedPath = fullPath;
            return true;
        }
        stream.close();
        stream.clear();
    }

    return false;
}

inline std::string getLogDirectory()
{
    std::ofstream test;
    std::string resolved;
    if (tryOpenLogStream(test, "test_perm.tmp", resolved))
    {
        test.close();
        std::error_code ec;
        std::filesystem::remove(resolved, ec);
        return resolved.substr(0, resolved.find_last_of("\\/"));
    }
    return "C:\\Users\\Public";
}

class Logger
{
public:
    static Logger& get()
    {
        static Logger instance;
        return instance;
    }

    void init()
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_logFile.is_open())
        {
            return;
        }

        std::string resolved;
        if (tryOpenLogStream(m_logFile, "butils.log", resolved))
        {
            m_initialized = true;
        }
    }

    template<typename... Args>
    void log(std::format_string<Args...> fmt, Args&&... args)
    {
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        write(msg);
    }

    void write(std::string_view msg)
    {
        std::lock_guard<std::mutex> lk(m_mutex);

        // 1. Write to standard output
        std::cout << msg << "\n";

        // 2. Write to log file
        if (m_logFile.is_open())
        {
            m_logFile << msg << "\n";
        }

        // 3. Write to OutputDebugString for debugger / DebugView
        std::string dbg = std::string(msg) + "\n";
        OutputDebugStringA(dbg.c_str());
    }

    void flush()
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        std::cout.flush();
        if (m_logFile.is_open())
        {
            m_logFile.flush();
        }
    }

    void close()
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_logFile.is_open())
        {
            m_logFile.flush();
            m_logFile.close();
        }
        m_initialized = false;
    }

private:
    Logger() = default;
    ~Logger()
    {
        close();
    }

    std::mutex    m_mutex;
    std::ofstream m_logFile;
    bool          m_initialized = false;
};

// Global helper functions
inline std::atomic<bool> g_verbose{false};

inline void setVerbose(bool enable) noexcept
{
    g_verbose.store(enable, std::memory_order_relaxed);
}

[[nodiscard]] inline bool isVerbose() noexcept
{
    return g_verbose.load(std::memory_order_relaxed);
}

template<typename... Args>
inline void log(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::get().log(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void logDebug(std::format_string<Args...> fmt, Args&&... args)
{
    if (g_verbose.load(std::memory_order_relaxed))
    {
        Logger::get().log(fmt, std::forward<Args>(args)...);
    }
}

inline void flush()
{
    Logger::get().flush();
}

} // namespace SDK::Log
