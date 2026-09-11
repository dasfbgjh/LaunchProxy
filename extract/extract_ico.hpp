#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <map>

struct ICONDIR {
    WORD idReserved;
    WORD idType;
    WORD idCount;
};

struct ICONDIRENTRY {
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    DWORD dwImageOffset;
};

struct GRPICONDIR {
    WORD idReserved;
    WORD idType;
    WORD nCount;
};

struct GRPICONDIRENTRY {
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    WORD nID;
};

typedef GRPICONDIR *LPGRPICONDIR;
typedef GRPICONDIRENTRY *LPGRPICONDIRENTRY;

BOOL CALLBACK EnumIconResourcesProc( HMODULE hModule, LPCSTR lpType, LPSTR lpName, LONG_PTR lParam ) {
    auto *iconEntries = reinterpret_cast<std::vector<std::vector<BYTE>> *>( lParam );

    HRSRC hIconRes = FindResource( hModule, lpName, RT_ICON );
    if ( !hIconRes ) {
        return TRUE;
    }

    HGLOBAL hIconData = LoadResource( hModule, hIconRes );
    if ( !hIconData ) {
        return TRUE;
    }

    DWORD size = SizeofResource( hModule, hIconRes );
    LPVOID pData = LockResource( hIconData );
    if ( !pData || size == 0 ) {
        FreeResource( hIconData );
        return TRUE;
    }

    std::vector<BYTE> entryData( size );
    memcpy( entryData.data(), pData, size );
    iconEntries->push_back( std::move( entryData ) );

    FreeResource( hIconData );
    return TRUE;
}

BOOL CALLBACK EnumIconsProc( HMODULE hModule, LPCSTR lpType, LPSTR lpName, LONG_PTR lParam ) {
    auto *iconEntries = reinterpret_cast<std::vector<std::vector<BYTE>> *>( lParam );
    
    HRSRC hResInfo = FindResource( hModule, lpName, RT_GROUP_ICON );
    if ( !hResInfo ) {
        return TRUE;
    }

    HGLOBAL hRes = LoadResource( hModule, hResInfo );
    if ( !hRes ) {
        return TRUE;
    }

    LPGRPICONDIR pGrpDir = (LPGRPICONDIR)LockResource( hRes );
    if ( !pGrpDir ) {
        FreeResource( hRes );
        return TRUE;
    }

    LPGRPICONDIRENTRY pGrpEntries = reinterpret_cast<LPGRPICONDIRENTRY>( 
        reinterpret_cast<LPBYTE>( pGrpDir ) + sizeof( GRPICONDIR ) );

    for ( int i = 0; i < pGrpDir->nCount; ++i ) {
        HRSRC hIconRes = FindResource( hModule, MAKEINTRESOURCE( pGrpEntries[i].nID ), RT_ICON );
        if ( !hIconRes ) {
            continue;
        }

        HGLOBAL hIconData = LoadResource( hModule, hIconRes );
        if ( !hIconData ) {
            continue;
        }

        DWORD size = SizeofResource( hModule, hIconRes );
        LPVOID pData = LockResource( hIconData );
        if ( !pData || size == 0 ) {
            FreeResource( hIconData );
            continue;
        }

        std::vector<BYTE> entryData( size );
        memcpy( entryData.data(), pData, size );
        iconEntries->push_back( std::move( entryData ) );

        FreeResource( hIconData );
    }

    FreeResource( hRes );
    return TRUE;
}

bool ExtractAllIconsToIco( const std::string &exePath, const std::string &icoPath ) {
    std::cout << "  Loading executable: " << exePath << std::endl;
    HMODULE hExe = LoadLibraryEx( exePath.c_str(), NULL, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE );
    if ( !hExe ) {
        std::cout << "    Trying fallback mode..." << std::endl;
        hExe = LoadLibraryEx( exePath.c_str(), NULL, LOAD_LIBRARY_AS_DATAFILE );
    }
    if ( !hExe ) {
        std::cerr << "    Error: load exe failed, error: " << GetLastError() << std::endl;
        return false;
    }
    std::cout << "    Loaded successfully" << std::endl;

    std::vector<std::vector<BYTE>> allIconEntries;
    
    std::cout << "  Enumerating icon resources..." << std::endl;
    EnumResourceNames( hExe, RT_GROUP_ICON, EnumIconsProc, (LONG_PTR)&allIconEntries );
    EnumResourceNames( hExe, RT_ICON, EnumIconResourcesProc, (LONG_PTR)&allIconEntries );

    FreeLibrary( hExe );

    std::cout << "    Found " << allIconEntries.size() << " icon resources" << std::endl;
    
    if ( allIconEntries.empty() ) {
        std::cerr << "    Error: no icon found in exe" << std::endl;
        return false;
    }

    std::map<std::pair<int, int>, std::vector<BYTE>> bestIcons;
    std::cout << "  Processing and deduplicating icons..." << std::endl;
    for ( const auto &entry : allIconEntries ) {
        if ( entry.size() < 12 ) {
            continue;
        }
        
        int w = 0, h = 0;
        
        if ( entry.size() >= 8 && entry[0] == 0x89 && entry[1] == 0x50 && entry[2] == 0x4E && entry[3] == 0x47 ) {
            if ( entry.size() >= 24 ) {
                w = ( entry[16] << 24 ) | ( entry[17] << 16 ) | ( entry[18] << 8 ) | entry[19];
                h = ( entry[20] << 24 ) | ( entry[21] << 16 ) | ( entry[22] << 8 ) | entry[23];
            }
        } else if ( entry.size() >= 40 ) {
            DWORD dibSize = *reinterpret_cast<const DWORD *>( &entry[0] );
            if ( dibSize == 40 ) {
                w = *reinterpret_cast<const LONG *>( &entry[4] );
                h = *reinterpret_cast<const LONG *>( &entry[8] ) / 2;
            }
        }
        
        if ( w == 0 || h == 0 ) {
            continue;
        }
        
        auto key = std::make_pair( w, h );
        
        auto existing = bestIcons.find( key );
        if ( existing == bestIcons.end() ) {
            bestIcons[key] = entry;
        } else {
            if ( entry.size() > existing->second.size() ) {
                bestIcons[key] = entry;
            }
        }
    }

    std::vector<std::vector<BYTE>> filteredIcons;
    for ( const auto &pair : bestIcons ) {
        filteredIcons.push_back( pair.second );
    }

    std::sort( filteredIcons.begin(), filteredIcons.end(), []( const std::vector<BYTE> &a, const std::vector<BYTE> &b ) {
        int widthA = 0, heightA = 0, widthB = 0, heightB = 0;
        
        if ( a.size() >= 4 && a[0] == 0x89 && a[1] == 0x50 && a[2] == 0x4E && a[3] == 0x47 && a.size() >= 24 ) {
            widthA = ( a[16] << 24 ) | ( a[17] << 16 ) | ( a[18] << 8 ) | a[19];
            heightA = ( a[20] << 24 ) | ( a[21] << 16 ) | ( a[22] << 8 ) | a[23];
        } else if ( a.size() >= 40 ) {
            DWORD dibSize = *reinterpret_cast<const DWORD *>( &a[0] );
            if ( dibSize == 40 ) {
                widthA = *reinterpret_cast<const LONG *>( &a[4] );
                heightA = *reinterpret_cast<const LONG *>( &a[8] ) / 2;
            }
        }
        
        if ( b.size() >= 4 && b[0] == 0x89 && b[1] == 0x50 && b[2] == 0x4E && b[3] == 0x47 && b.size() >= 24 ) {
            widthB = ( b[16] << 24 ) | ( b[17] << 16 ) | ( b[18] << 8 ) | b[19];
            heightB = ( b[20] << 24 ) | ( b[21] << 16 ) | ( b[22] << 8 ) | b[23];
        } else if ( b.size() >= 40 ) {
            DWORD dibSize = *reinterpret_cast<const DWORD *>( &b[0] );
            if ( dibSize == 40 ) {
                widthB = *reinterpret_cast<const LONG *>( &b[4] );
                heightB = *reinterpret_cast<const LONG *>( &b[8] ) / 2;
            }
        }
        
        return widthA * heightA > widthB * heightB;
    } );

    std::cout << "  Selected " << filteredIcons.size() << " unique icons:" << std::endl;
    for ( const auto &entry : filteredIcons ) {
        int w = 0, h = 0, bitCount = 0;
        if ( entry.size() >= 4 && entry[0] == 0x89 && entry[1] == 0x50 && entry[2] == 0x4E && entry[3] == 0x47 ) {
            if ( entry.size() >= 24 ) {
                w = ( entry[16] << 24 ) | ( entry[17] << 16 ) | ( entry[18] << 8 ) | entry[19];
                h = ( entry[20] << 24 ) | ( entry[21] << 16 ) | ( entry[22] << 8 ) | entry[23];
            }
            bitCount = 32;
        } else if ( entry.size() >= 40 ) {
            DWORD dibSize = *reinterpret_cast<const DWORD *>( &entry[0] );
            if ( dibSize == 40 ) {
                w = *reinterpret_cast<const LONG *>( &entry[4] );
                h = *reinterpret_cast<const LONG *>( &entry[8] ) / 2;
                bitCount = *reinterpret_cast<const WORD *>( &entry[14] );
            }
        }
        std::cout << "    " << w << "x" << h << ", " << bitCount << "-bit, " << entry.size() << " bytes" << std::endl;
    }

    std::ofstream outFile( icoPath.c_str(), std::ios::binary );
    if ( !outFile ) {
        std::cerr << "    Error: create ico file failed: " << icoPath << std::endl;
        return false;
    }
    std::cout << "  Writing ICO file: " << icoPath << std::endl;

    ICONDIR dir = { 0, 1, (WORD)filteredIcons.size() };
    outFile.write( reinterpret_cast<char *>( &dir ), sizeof( dir ) );

    DWORD currentOffset = sizeof( ICONDIR ) + ( sizeof( ICONDIRENTRY ) * filteredIcons.size() );

    for ( const auto &entry : filteredIcons ) {
        ICONDIRENTRY dirEntry = { 0 };
        
        if ( entry.size() >= 4 && entry[0] == 0x89 && entry[1] == 0x50 && entry[2] == 0x4E && entry[3] == 0x47 ) {
            int w = 0, h = 0;
            if ( entry.size() >= 24 ) {
                w = ( entry[16] << 24 ) | ( entry[17] << 16 ) | ( entry[18] << 8 ) | entry[19];
                h = ( entry[20] << 24 ) | ( entry[21] << 16 ) | ( entry[22] << 8 ) | entry[23];
            }
            dirEntry.bWidth = ( w >= 256 ) ? 0 : static_cast<BYTE>( w );
            dirEntry.bHeight = ( h >= 256 ) ? 0 : static_cast<BYTE>( h );
            dirEntry.bColorCount = 0;
            dirEntry.bReserved = 0;
            dirEntry.wPlanes = 1;
            dirEntry.wBitCount = 32;
        } else if ( entry.size() >= 40 ) {
            DWORD dibSize = *reinterpret_cast<const DWORD *>( &entry[0] );
            if ( dibSize == 40 ) {
                int w = *reinterpret_cast<const LONG *>( &entry[4] );
                int h = *reinterpret_cast<const LONG *>( &entry[8] ) / 2;
                dirEntry.bWidth = ( w >= 256 ) ? 0 : static_cast<BYTE>( w );
                dirEntry.bHeight = ( h >= 256 ) ? 0 : static_cast<BYTE>( h );
                dirEntry.bColorCount = 0;
                dirEntry.bReserved = 0;
                dirEntry.wPlanes = *reinterpret_cast<const WORD *>( &entry[12] );
                dirEntry.wBitCount = *reinterpret_cast<const WORD *>( &entry[14] );
            }
        } else {
            dirEntry.bWidth = entry[0];
            dirEntry.bHeight = entry[1];
            dirEntry.bColorCount = entry[2];
            dirEntry.bReserved = entry[3];
            dirEntry.wPlanes = *reinterpret_cast<const WORD *>( &entry[4] );
            dirEntry.wBitCount = *reinterpret_cast<const WORD *>( &entry[6] );
        }
        
        dirEntry.dwBytesInRes = static_cast<DWORD>( entry.size() );
        dirEntry.dwImageOffset = currentOffset;

        outFile.write( reinterpret_cast<char *>( &dirEntry ), sizeof( dirEntry ) );
        currentOffset += dirEntry.dwBytesInRes;
    }

    for ( const auto &entry : filteredIcons ) {
        outFile.write( reinterpret_cast<const char *>( entry.data() ), entry.size() );
    }

    outFile.close();
    std::cout << "    Successfully wrote " << filteredIcons.size() << " icons" << std::endl;
    return true;
}