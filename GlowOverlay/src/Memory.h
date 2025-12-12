#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <optional>
#include <vector>

class Memory {
public:
    Memory() = default;
    explicit Memory(const std::wstring& processName) { Attach(processName); }

    bool Attach(const std::wstring& processName);
    bool IsAttached() const { return processHandle_ != nullptr; }

    template <typename T>
    std::optional<T> Read(uintptr_t address) const {
        if (!processHandle_) return std::nullopt;
        T buffer{};
        SIZE_T bytesRead{};
        if (ReadProcessMemory(processHandle_, reinterpret_cast<LPCVOID>(address), &buffer, sizeof(T), &bytesRead) && bytesRead == sizeof(T)) {
            return buffer;
        }
        return std::nullopt;
    }

    bool ReadBytes(uintptr_t address, void* buffer, SIZE_T size) const;
    uintptr_t GetModuleBase(const std::wstring& moduleName) const;

private:
    HANDLE processHandle_{nullptr};
    DWORD processId_{};
};
