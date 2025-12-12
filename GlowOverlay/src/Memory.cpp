#include "Memory.h"
#include <Psapi.h>

bool Memory::Attach(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
        if (processName == entry.szExeFile) {
            processId_ = entry.th32ProcessID;
            processHandle_ = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId_);
            break;
        }
    }

    CloseHandle(snapshot);
    return processHandle_ != nullptr;
}

bool Memory::ReadBytes(uintptr_t address, void* buffer, SIZE_T size) const {
    if (!processHandle_) return false;
    SIZE_T bytesRead{};
    return ReadProcessMemory(processHandle_, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead) && bytesRead == size;
}

uintptr_t Memory::GetModuleBase(const std::wstring& moduleName) const {
    if (!processHandle_) return 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId_);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W mod{};
    mod.dwSize = sizeof(mod);
    uintptr_t result = 0;

    for (BOOL ok = Module32FirstW(snapshot, &mod); ok; ok = Module32NextW(snapshot, &mod)) {
        if (moduleName == mod.szModule) {
            result = reinterpret_cast<uintptr_t>(mod.modBaseAddr);
            break;
        }
    }

    CloseHandle(snapshot);
    return result;
}
