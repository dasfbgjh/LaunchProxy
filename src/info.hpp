#ifndef INFO_H
#define INFO_H

// 程序命令
#define PROGRAM_COMMAND "1.exe"

// 工作目录
#define PROGRAM_WORKING_DIR "."

// 是否继承环境变量
#define PROGRAM_INHERIT_ENV true

// 环境变量
// clang-format off
#define PROGRAM_ENV_VARS \
    {  {"PATH", "D:\\test;"}  }
// clang-format on

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <windows.h>

std::filesystem::path g_exePath;

void initPath( int argc, const char *argv[] ) {
    auto exeFullPath = std::filesystem::absolute( argv[0] ).string();
    auto fullPath = std::filesystem::path( exeFullPath );
    g_exePath = fullPath.parent_path();
}

std::string buildEnvString( const std::vector<std::pair<std::string, std::string>> &envVars, bool inheritEnv ) {
    std::vector<std::string> envStrings;
    if ( inheritEnv ) {
        char *envBlock = GetEnvironmentStrings();
        if ( envBlock ) {
            char *current = envBlock;
            while ( *current ) {
                std::string envStr( current );
                envStrings.push_back( envStr );
                current += envStr.length() + 1;
            }
            FreeEnvironmentStrings( envBlock );
        }
    }

    for ( const auto &pair : envVars ) {
        envStrings.push_back( pair.first + "=" + pair.second );
    }

    std::string result;
    for ( const auto &str : envStrings ) {
        result += str + '\0';
    }
    result += '\0';

    return result;
}

bool launchProcess(
    const std::string &command,
    const std::string &workingDirectory,
    const std::vector<std::pair<std::string, std::string>> &envVars,
    const std::string &additionalArgs = "",
    bool inheritEnv = true ) {

    std::string fullArguments = command;
    if ( !additionalArgs.empty() ) {
        if ( !fullArguments.empty() )
            fullArguments += " ";
        fullArguments += additionalArgs;
    }

    std::filesystem::path wd = workingDirectory;
    if ( !wd.empty() && !wd.is_absolute() ) {
        std::filesystem::path resolvedWd = g_exePath / workingDirectory;
        if ( std::filesystem::exists( resolvedWd ) ) {
            wd = resolvedWd;
        }
    }
    std::string workingDirStr = wd.string();

    std::string envPtr = buildEnvString( envVars, inheritEnv );

    STARTUPINFOA si = { sizeof( si ) };
    PROCESS_INFORMATION pi = { 0 };
    bool success = CreateProcessA(
        nullptr,
        const_cast<char *>( fullArguments.c_str() ),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        const_cast<char *>( envPtr.c_str() ),
        const_cast<char *>( workingDirStr.c_str() ),
        &si,
        &pi );

    if ( success ) {
        CloseHandle( pi.hThread );
        CloseHandle( pi.hProcess );
        return true;
    }

    return false;
}

bool run( int argc, const char *argv[] ) {
    initPath( argc, argv );

    std::string additionalArgs;
    for ( int i = 1; i < argc; ++i ) {
        if ( i > 1 ) {
            additionalArgs += " ";
        }
        additionalArgs += argv[i];
    }

    bool success = launchProcess(
        PROGRAM_COMMAND,
        PROGRAM_WORKING_DIR,
        PROGRAM_ENV_VARS,
        additionalArgs,
        PROGRAM_INHERIT_ENV );

    if ( success )
        std::cout << "Process launched successfully" << std::endl;
    else
        std::cerr << "Failed to launch process. Error: " << GetLastError() << std::endl;

    return success;
}
#endif