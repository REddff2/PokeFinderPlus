#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>
#include <vector>

namespace
{
std::wstring quoteArgument(const std::wstring &argument)
{
    std::wstring quoted = L"\"";
    size_t backslashes = 0;
    for (wchar_t character : argument)
    {
        if (character == L'\\')
        {
            ++backslashes;
        }
        else if (character == L'\"')
        {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted += L'\"';
            backslashes = 0;
        }
        else
        {
            quoted.append(backslashes, L'\\');
            quoted += character;
            backslashes = 0;
        }
    }
    quoted.append(backslashes * 2, L'\\');
    quoted += L'\"';
    return quoted;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int argumentCount = 0;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (!arguments)
    {
        return ERROR_INVALID_PARAMETER;
    }

    wchar_t modulePath[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        LocalFree(arguments);
        return GetLastError();
    }

    const std::filesystem::path launcherDirectory = std::filesystem::path(modulePath).parent_path();
    const std::filesystem::path parentDirectory = launcherDirectory.parent_path();
    const std::filesystem::path applicationPath = launcherDirectory / L"PokeFinderPlusApp.exe";

    wchar_t oldPath[32768];
    DWORD oldPathLength = GetEnvironmentVariableW(L"PATH", oldPath, 32768);
    std::wstring path = parentDirectory.wstring();
    if (oldPathLength > 0 && oldPathLength < 32768)
    {
        path += L";";
        path.append(oldPath, oldPathLength);
    }
    SetEnvironmentVariableW(L"PATH", path.c_str());

    std::wstring commandLine = quoteArgument(applicationPath.wstring());
    for (int index = 1; index < argumentCount; ++index)
    {
        commandLine += L" ";
        commandLine += quoteArgument(arguments[index]);
    }
    LocalFree(arguments);

    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    if (!CreateProcessW(applicationPath.c_str(), mutableCommandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        launcherDirectory.c_str(), &startupInfo, &processInfo))
    {
        MessageBoxW(nullptr, L"Unable to launch PokeFinderPlusApp.exe.", L"PokeFinder+", MB_ICONERROR);
        return GetLastError();
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return static_cast<int>(exitCode);
}
