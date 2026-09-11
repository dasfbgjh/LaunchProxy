#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <windows.h>
#include "extract_ico.hpp"

bool isValidUtf8( const std::string &str ) {
    int i = 0;
    while ( i < str.size() ) {
        if ( ( str[i] & 0x80 ) == 0 ) {
            i++;
        } else if ( ( str[i] & 0xE0 ) == 0xC0 ) {
            if ( i + 1 >= str.size() )
                return false;
            if ( ( str[i + 1] & 0xC0 ) != 0x80 )
                return false;
            i += 2;
        } else if ( ( str[i] & 0xF0 ) == 0xE0 ) {
            if ( i + 2 >= str.size() )
                return false;
            if ( ( str[i + 1] & 0xC0 ) != 0x80 )
                return false;
            if ( ( str[i + 2] & 0xC0 ) != 0x80 )
                return false;
            i += 3;
        } else if ( ( str[i] & 0xF8 ) == 0xF0 ) {
            if ( i + 3 >= str.size() )
                return false;
            if ( ( str[i + 1] & 0xC0 ) != 0x80 )
                return false;
            if ( ( str[i + 2] & 0xC0 ) != 0x80 )
                return false;
            if ( ( str[i + 3] & 0xC0 ) != 0x80 )
                return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

std::string gbkToUtf8( const std::string &gbkStr ) {
    if ( isValidUtf8( gbkStr ) ) {
        return gbkStr;
    }

    int utf16Len = MultiByteToWideChar( CP_ACP, 0, gbkStr.c_str(), -1, nullptr, 0 );
    if ( utf16Len == 0 ) {
        return gbkStr;
    }

    std::wstring utf16Str( utf16Len, L'\0' );
    MultiByteToWideChar( CP_ACP, 0, gbkStr.c_str(), -1, &utf16Str[0], utf16Len );

    int utf8Len = WideCharToMultiByte( CP_UTF8, 0, utf16Str.c_str(), -1, nullptr, 0, nullptr, nullptr );
    if ( utf8Len == 0 ) {
        return gbkStr;
    }

    std::string utf8Str( utf8Len, '\0' );
    WideCharToMultiByte( CP_UTF8, 0, utf16Str.c_str(), -1, &utf8Str[0], utf8Len, nullptr, nullptr );

    if ( !utf8Str.empty() && utf8Str.back() == '\0' ) {
        utf8Str.pop_back();
    }

    return utf8Str;
}

bool executePowerShellCommand( const std::string &command, std::string &output ) {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof( SECURITY_ATTRIBUTES );
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hRead = NULL, hWrite = NULL;
    if ( !CreatePipe( &hRead, &hWrite, &saAttr, 0 ) ) {
        return false;
    }

    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    ZeroMemory( &si, sizeof( STARTUPINFOA ) );
    si.cb = sizeof( STARTUPINFOA );
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    std::string cmd = "powershell.exe -NoProfile -Command \"";
    cmd += command;
    cmd += "\"";

    if ( !CreateProcessA( NULL, (LPSTR)cmd.c_str(),
                          NULL,
                          NULL,
                          TRUE, 0,
                          NULL,
                          NULL,
                          &si,
                          &pi ) ) {
        CloseHandle( hRead );
        CloseHandle( hWrite );
        return false;
    }

    CloseHandle( hWrite );

    std::vector<char> buffer( 8192 );
    DWORD bytesRead;
    while ( ReadFile( hRead, buffer.data(), buffer.size(), &bytesRead, NULL ) && bytesRead > 0 ) {
        output.append( buffer.data(), bytesRead );
    }

    CloseHandle( hRead );
    WaitForSingleObject( pi.hProcess, INFINITE );
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    return true;
}

std::string trim( const std::string &str ) {
    const std::string split = "--------";
    int pos = str.find_first_of( split );
    if ( pos == std::string::npos )
        return "";
    std::string str2 = str.substr( pos + split.size() );
    pos = str2.find_first_not_of( "- \t\n\r" );
    if ( pos == std::string::npos )
        return "";
    int pos1 = str2.find_last_not_of( "- \t\n\r" );
    if ( pos1 == std::string::npos )
        return "";
    str2 = str2.substr( pos, pos1 + 1 - pos );
    return str2;
}

std::string getGuardName() {
    auto headerName = std::filesystem::path( INFO_FILE ).filename();
    std::string guardName = headerName.string();
    std::string res;
    for ( char &c : guardName ) {
        if ( c == '.' ) {
            c = '_';
        } else {
            c = std::toupper( c );
        }
        res += c;
    }
    return res;
}

void replaceAll( std::string &str, const std::string &from, const std::string &to ) {
    size_t pos = 0;
    while ( ( pos = str.find( from, pos ) ) != std::string::npos ) {
        str.replace( pos, from.length(), to );
        pos += to.length();
    }
}

std::string addDefine( const std::string &name, const std::string &lable, std::string &value, bool addQuotation = true ) {
    std::string cmd = "(Get-Item '" + std::string( EXE_FILE ) + "').VersionInfo | Select-Object -Property " + name;

    std::string outStr;
    if ( !executePowerShellCommand( cmd, outStr ) ) {
        std::cout << "Failed to execute PowerShell command: "
                  << name << std::endl;
    }
    value = trim( outStr );
    if ( addQuotation )
        return "#define " + lable + " \"" + value + "\"\n\n";

    std::string str = value;
    replaceAll( str, ".", ", " );
    return "#define " + lable + " " + str + "\n\n";
}

std::string manifest( const std::string &name,
                      const std::string &version,
                      const std::string &description ) {
    std::string str1 = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
)";

    std::string str2 = R"(  <!-- 3. 启用 Windows 视觉样式 (ComCtl32 v6) -->
  <!-- 没有此项，Win32 控件将显示为 Windows 95/2000 经典样式 -->
  <dependency>
    <dependentAssembly>
      <assemblyIdentity
          type="win32"
          name="Microsoft.Windows.Common-Controls"
          version="6.0.0.0"
          processorArchitecture="*"
          publicKeyToken="6595b64144ccf1df"
          language="*" />
    </dependentAssembly>
  </dependency>

  <!-- 4. UAC 权限声明 -->
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <!-- level 可选值: asInvoker, highestAvailable, requireAdministrator -->
        <requestedExecutionLevel level="asInvoker" uiAccess="false" />
      </requestedPrivileges>
    </security>
  </trustInfo>

  <!-- 5. 高 DPI 感知 (Windows 10/11) -->
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <!-- PerMonitorV2 是推荐的现代 DPI 感知模式 -->
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
      <!-- 兼容旧版系统的回退设置 -->
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
    </windowsSettings>
  </application>

  <!-- 6. 系统兼容性声明 -->
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <!-- Windows 10 / 11 -->
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}" />
      <!-- Windows 8.1 -->
      <supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}" />
      <!-- Windows 8 -->
      <supportedOS Id="{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}" />
      <!-- Windows 7 -->
      <supportedOS Id="{35138b9a-5d96-4fbd-8e2d-a2440225f93a}" />
    </application>
  </compatibility>

</assembly>
)";

    std::string out = str1;
    out.append( "    <assemblyIdentity\n" );
    out.append( "        type=\"win32\"\n" );
    out.append( "        name=\"" + gbkToUtf8( name ) + "\"\n" );
    out.append( "        version=\"" + gbkToUtf8( version ) + "\"\n" );
    out.append( "        processorArchitecture=\"amd64\"\n" );
    out.append( "    />\n" );
    out.append( "    <description>" + gbkToUtf8( description ) + "</description>\n" );
    out.append( str2 );

    return out;
}

int main( int argc, char const *argv[] ) {
    std::cout << "=== Extracting resources from: " << EXE_FILE << " ===" << std::endl;

    std::ofstream headFile( INFO_FILE, std::ios::binary | std::ios::out );
    std::cout << "Creating info header: " << INFO_FILE << std::endl;
    headFile << "#ifndef " << getGuardName() << "_H\n";
    headFile << "#define " << getGuardName() << "_H\n\n";

    std::string value;
    std::string name;
    std::string version;
    std::string description;

    headFile << addDefine( "CompanyName", "COMPANY_NAME", value );
    std::cout << "  COMPANY_NAME: " << value << std::endl;

    headFile << addDefine( "LegalCopyright", "LEGAL_COPYRIGHT", value );
    std::cout << "  LEGAL_COPYRIGHT: " << value << std::endl;

    headFile << addDefine( "OriginalFilename", "ORIGINAL_FILE_NAME", value );
    std::cout << "  ORIGINAL_FILE_NAME: " << value << std::endl;

    headFile << addDefine( "ProductName", "PRODUCT_NAME", value );
    name = value;
    std::cout << "  PRODUCT_NAME: " << value << std::endl;

    headFile << addDefine( "OriginalFilename", "INTERNAL_NAME", value );
    std::cout << "  INTERNAL_NAME: " << value << std::endl;

    headFile << addDefine( "FileDescription", "FILE_DESCRIPTION", value );
    description = value;
    std::cout << "  FILE_DESCRIPTION: " << value << std::endl;

    headFile << addDefine( "FileVersionRaw", "APP_FILE_VERSION", value, false );
    version = value;
    std::cout << "  APP_FILE_VERSION: " << value << std::endl;

    headFile << addDefine( "ProductVersionRaw", "APP_PRODUCT_VERSION", value, false );
    std::cout << "  APP_PRODUCT_VERSION: " << value << std::endl;

    headFile << addDefine( "FileVersionRaw", "APP_FILE_VERSION_STR", value );
    std::cout << "  APP_FILE_VERSION_STR: " << value << std::endl;

    headFile << addDefine( "ProductVersionRaw", "APP_PRODUCT_VERSION_STR", value );
    std::cout << "  APP_PRODUCT_VERSION_STR: " << value << std::endl;

    headFile << "\n#endif\n";
    std::cout << "Info header created successfully" << std::endl;

    std::ofstream manifestFile( MANIFEST_FILE, std::ios::binary | std::ios::out );
    std::cout << "Creating manifest file: " << MANIFEST_FILE << std::endl;
    manifestFile << manifest( name, version, description );
    std::cout << "Manifest file created successfully" << std::endl;

    std::cout << "Extracting icons to: " << ICO_FILE << std::endl;
    ExtractAllIconsToIco( EXE_FILE, ICO_FILE );

    std::cout << "=== All resources extracted successfully ===" << std::endl;
    return 0;
}