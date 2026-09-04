#pragma once
#include "JsonUtils.h"
#include <atomic>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")

namespace SDK::Http
{

struct HttpResponse
{
    bool success = false;
    uint32_t statusCode = 0;
    std::string body;
};

struct UrlComponents
{
    bool isHttps = true;
    std::wstring host;
    uint16_t port = 443;
    std::wstring path = L"/";
};

inline UrlComponents parseUrl(std::string_view rawUrl)
{
    UrlComponents comp;
    std::string_view u = rawUrl;

    if (u.starts_with("https://") || u.starts_with("HTTPS://"))
    {
        comp.isHttps = true;
        comp.port = 443;
        u.remove_prefix(8);
    }
    else if (u.starts_with("http://") || u.starts_with("HTTP://"))
    {
        comp.isHttps = false;
        comp.port = 80;
        u.remove_prefix(7);
    }

    size_t slashPos = u.find('/');
    std::string_view hostPort = (slashPos != std::string_view::npos) ? u.substr(0, slashPos) : u;
    std::string_view pathQuery = (slashPos != std::string_view::npos) ? u.substr(slashPos) : "/";

    size_t colonPos = hostPort.find(':');
    if (colonPos != std::string_view::npos)
    {
        std::string h(hostPort.substr(0, colonPos));
        comp.host = std::wstring(h.begin(), h.end());
        try
        {
            comp.port = static_cast<uint16_t>(std::stoul(std::string(hostPort.substr(colonPos + 1))));
        }
        catch (...) {}
    }
    else
    {
        std::string h(hostPort);
        comp.host = std::wstring(h.begin(), h.end());
    }

    std::string p(pathQuery);
    comp.path = std::wstring(p.begin(), p.end());
    return comp;
}

inline HttpResponse execCurl(
    std::string_view method,
    std::string_view url,
    std::string_view body = "",
    std::string_view contentType = "",
    uint32_t timeoutSec = 5,
    const std::vector<std::string>& headers = {}
)
{
    static std::mutex s_execCurlMutex;
    std::lock_guard<std::mutex> lk(s_execCurlMutex);

    HttpResponse response;

    HANDLE hStdInRead = NULL, hStdInWrite = NULL;
    HANDLE hStdOutRead = NULL, hStdOutWrite = NULL;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0))
    {
        return response;
    }
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&hStdInRead, &hStdInWrite, &saAttr, 0))
    {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return response;
    }
    SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0);

    if (!body.empty())
    {
        DWORD bytesWritten = 0;
        WriteFile(hStdInWrite, body.data(), static_cast<DWORD>(body.length()), &bytesWritten, NULL);
    }
    CloseHandle(hStdInWrite);

    std::wstring headerArgs;
    if (!contentType.empty())
    {
        headerArgs += std::format(L" -H \"Content-Type: {}\"", std::wstring(contentType.begin(), contentType.end()));
    }
    for (const std::string& h : headers)
    {
        headerArgs += std::format(L" -H \"{}\"", std::wstring(h.begin(), h.end()));
    }

    std::wstring wUrl(url.begin(), url.end());
    std::wstring wMethod(method.begin(), method.end());

    std::wstring cmd;
    if (!body.empty())
    {
        cmd = std::format(
            L"curl.exe -k -s -m {} -X {} \"{}\"{} --data-binary @- -w \"\\n%{{http_code}}\"",
            timeoutSec, wMethod, wUrl, headerArgs
        );
    }
    else
    {
        cmd = std::format(
            L"curl.exe -k -s -m {} -X {} \"{}\"{} -w \"\\n%{{http_code}}\"",
            timeoutSec, wMethod, wUrl, headerArgs
        );
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hStdOutWrite;
    si.hStdOutput = hStdOutWrite;
    si.hStdInput = hStdInRead;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL success = CreateProcessW(
        NULL, cmd.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi
    );

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdInRead);

    if (!success)
    {
        CloseHandle(hStdOutRead);
        return response;
    }

    std::string rawOutput;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        rawOutput.append(buffer, bytesRead);
    }

    DWORD waitRes = WaitForSingleObject(pi.hProcess, (timeoutSec + 1) * 1000);
    if (waitRes == WAIT_TIMEOUT)
    {
        TerminateProcess(pi.hProcess, 1);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdOutRead);

    size_t lastNewline = rawOutput.find_last_of('\n');
    if (lastNewline != std::string::npos && lastNewline + 1 < rawOutput.size())
    {
        std::string codeStr = rawOutput.substr(lastNewline + 1);
        try
        {
            response.statusCode = static_cast<uint32_t>(std::stoul(codeStr));
        }
        catch (...)
        {
            response.statusCode = 0;
        }
        response.body = rawOutput.substr(0, lastNewline);
    }
    else
    {
        response.body = rawOutput;
        response.statusCode = (rawOutput.find("\"code\":200") != std::string::npos || rawOutput.find("\"data\":") != std::string::npos) ? 200 : 0;
    }

    response.success = (response.statusCode >= 200 && response.statusCode < 300) ||
                       (response.body.find("\"data\":") != std::string::npos);
    return response;
}

inline std::atomic<bool> s_preferCurl{false};

inline HttpResponse request(
    std::string_view method,
    std::string_view url,
    std::string_view body = "",
    std::string_view contentType = "",
    uint32_t timeoutMs = 5000,
    const std::vector<std::string>& headers = {}
)
{
    if (s_preferCurl.load(std::memory_order_relaxed))
    {
        HttpResponse curlRes = execCurl(method, url, body, contentType, (timeoutMs + 999) / 1000, headers);
        if (curlRes.success || !curlRes.body.empty())
        {
            return curlRes;
        }
    }

    UrlComponents u = parseUrl(url);
    HttpResponse response;

    HINTERNET hInternet = InternetOpenW(
        L"BedrockUtils/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG,
        nullptr,
        nullptr,
        0
    );

    if (!hInternet)
    {
        hInternet = InternetOpenW(
            L"BedrockUtils/1.0",
            INTERNET_OPEN_TYPE_DIRECT,
            nullptr,
            nullptr,
            0
        );
    }

    if (hInternet)
    {
        InternetSetOptionW(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
        InternetSetOptionW(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
        InternetSetOptionW(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

        HINTERNET hConnect = InternetConnectW(
            hInternet,
            u.host.c_str(),
            u.port,
            nullptr,
            nullptr,
            INTERNET_SERVICE_HTTP,
            0,
            0
        );

        if (hConnect)
        {
            DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
            if (u.isHttps)
            {
                flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
            }

            std::wstring wMethod(method.begin(), method.end());
            HINTERNET hRequest = HttpOpenRequestW(
                hConnect,
                wMethod.c_str(),
                u.path.c_str(),
                nullptr,
                nullptr,
                nullptr,
                flags,
                0
            );

            if (hRequest)
            {
                if (u.isHttps)
                {
                    DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                                     SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                                     SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                                     SECURITY_FLAG_IGNORE_REVOCATION;
                    InternetSetOptionW(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
                }

                std::wstring headerBlock;
                if (!contentType.empty())
                {
                    headerBlock += std::format(L"Content-Type: {}\r\n", std::wstring(contentType.begin(), contentType.end()));
                }
                for (const std::string& h : headers)
                {
                    headerBlock += std::format(L"{}\r\n", std::wstring(h.begin(), h.end()));
                }

                BOOL sendOk = HttpSendRequestW(
                    hRequest,
                    headerBlock.empty() ? nullptr : headerBlock.c_str(),
                    static_cast<DWORD>(headerBlock.length()),
                    body.empty() ? nullptr : const_cast<char*>(body.data()),
                    static_cast<DWORD>(body.length())
                );

                if (!sendOk)
                {
                    DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                                     SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                                     SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                                     SECURITY_FLAG_IGNORE_REVOCATION;
                    InternetSetOptionW(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
                    sendOk = HttpSendRequestW(
                        hRequest,
                        headerBlock.empty() ? nullptr : headerBlock.c_str(),
                        static_cast<DWORD>(headerBlock.length()),
                        body.empty() ? nullptr : const_cast<char*>(body.data()),
                        static_cast<DWORD>(body.length())
                    );
                }

                if (sendOk)
                {
                    DWORD statusCode = 0;
                    DWORD statusSize = sizeof(statusCode);
                    if (HttpQueryInfoW(
                        hRequest,
                        HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                        &statusCode,
                        &statusSize,
                        nullptr
                    ))
                    {
                        response.statusCode = statusCode;
                    }

                    char buf[4096];
                    DWORD read = 0;
                    while (InternetReadFile(hRequest, buf, sizeof(buf) - 1, &read) && read > 0)
                    {
                        buf[read] = '\0';
                        response.body.append(buf, read);
                    }

                    response.success = (response.statusCode >= 200 && response.statusCode < 300);
                }

                InternetCloseHandle(hRequest);
            }

            InternetCloseHandle(hConnect);
        }

        InternetCloseHandle(hInternet);
    }

    if (!response.success || response.statusCode == 404)
    {
        HttpResponse curlRes = execCurl(method, url, body, contentType, (timeoutMs + 999) / 1000, headers);
        if (curlRes.success || !curlRes.body.empty())
        {
            s_preferCurl.store(true, std::memory_order_relaxed);
            return curlRes;
        }
    }

    return response;
}

inline HttpResponse get(
    std::string_view url,
    uint32_t timeoutMs = 5000,
    const std::vector<std::string>& headers = {}
)
{
    return request("GET", url, "", "", timeoutMs, headers);
}

inline HttpResponse post(
    std::string_view url,
    std::string_view body,
    std::string_view contentType = "application/x-www-form-urlencoded",
    uint32_t timeoutMs = 5000,
    const std::vector<std::string>& headers = {}
)
{
    return request("POST", url, body, contentType, timeoutMs, headers);
}

inline HttpResponse postJson(
    std::string_view url,
    std::string_view jsonPayload,
    uint32_t timeoutMs = 5000,
    const std::vector<std::string>& headers = {}
)
{
    return request("POST", url, jsonPayload, "application/json", timeoutMs, headers);
}

inline HttpResponse postJson(
    std::wstring_view host,
    uint16_t port,
    std::wstring_view path,
    std::string_view jsonPayload,
    bool secure = true,
    uint32_t timeoutMs = 5000
)
{
    std::string scheme = secure ? "https" : "http";
    std::string h(host.begin(), host.end());
    std::string p(path.begin(), path.end());
    std::string fullUrl = std::format("{}://{}:{}{}", scheme, h, port, p);
    return postJson(fullUrl, jsonPayload, timeoutMs);
}

// JSON helpers forwarded from generic SDK::Json
using Json::escapeJson;
using Json::extractJsonString;
using Json::unescapeJsonString;

} // namespace SDK::Http
