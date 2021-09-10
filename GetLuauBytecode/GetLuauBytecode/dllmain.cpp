#include <iostream>
#include <Windows.h>
#include <thread>
#include <sstream>
#include <iomanip>

#pragma warning(disable : 4996) // Allow for use of freopen.

constexpr std::uintptr_t DESERIALIZE_OFFSET = 0x1EA7900; // This needs to be changed every time Roblox Studio updates.
constexpr int BYTECODE_ARG_OFFSET = 96; // RSP + BYTECODE_ARG_OFFSET is where the bytecode is.
constexpr int BYTECODE_SIZE_ARG_OFFSET = 112; // RSP + BYTECODE_SIZE_ARG_OFFSET is where the bytecode size/length is.
constexpr int OVERWRITE_INSTR_SIZE = 5; // The MOV instruction that is going to be replaced is OVERWRITE_INSTR_SIZE.

std::uintptr_t deserializeAddress;

void createConsole(const std::string& title) {
    {
        DWORD oldProtection;
        VirtualProtect(FreeConsole, sizeof(std::uint8_t), PAGE_EXECUTE_READWRITE, &oldProtection);

        *reinterpret_cast<std::uint8_t*>(FreeConsole) = 0xC3; // 0xC3 is the 'ret' instruction opcode.

        VirtualProtect(FreeConsole, sizeof(std::uint8_t), oldProtection, &oldProtection);
    }
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONIN$", "r", stdin);
    SetConsoleTitleA(title.c_str());
}

LONG WINAPI exceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    if (exceptionInfo->ContextRecord->Rip == deserializeAddress) {
        std::uint8_t* bytecode = reinterpret_cast<std::uint8_t*>(*reinterpret_cast<std::uintptr_t*>(exceptionInfo->ContextRecord->Rsp + BYTECODE_ARG_OFFSET));
        int bytecodeSize = *reinterpret_cast<int*>(exceptionInfo->ContextRecord->Rsp + BYTECODE_SIZE_ARG_OFFSET);
        std::stringstream bytecodeHexStream;
        std::cout << "Chunk name: " << reinterpret_cast<const char*>(exceptionInfo->ContextRecord->Rdx) << '\n';
        std::cout << "Bytecode size: " << bytecodeSize << '\n';
        for (int i = 0; i < bytecodeSize; i++) {
            bytecodeHexStream << std::setfill('0') << std::setw(2) << std::hex << (0xFF & static_cast<int>(bytecode[i])) << " ";
        }
        std::cout << "Bytecode: " << bytecodeHexStream.str() << '\n';

        /*__asm {
            mov [rsp+08], rcx
        }*/
        *reinterpret_cast<std::uintptr_t*>(exceptionInfo->ContextRecord->Rsp + 8) = exceptionInfo->ContextRecord->Rcx; // This is the original instruction that was replaced with 0xCC's.

        exceptionInfo->ContextRecord->Rip = deserializeAddress + OVERWRITE_INSTR_SIZE; // The mov instructed that was replaced was OVERWRITE_INSTR_SIZE bytes so we have to skip it to jump over the 0xCC's.
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

int main() {
    createConsole("Get Luau Bytecode");

    std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
    deserializeAddress = baseAddress + DESERIALIZE_OFFSET;

    std::cout << "Base address: " << std::hex << baseAddress << '\n';
    std::cout << "Deserialize address: " << deserializeAddress << std::dec << '\n';

    AddVectoredExceptionHandler(true, exceptionHandler);

    DWORD oldProtection;
    VirtualProtect(reinterpret_cast<LPVOID>(deserializeAddress), OVERWRITE_INSTR_SIZE, PAGE_EXECUTE_READWRITE, &oldProtection);

    std::memset(reinterpret_cast<void*>(deserializeAddress), 0xCC, OVERWRITE_INSTR_SIZE); // Using 0xCC ('int 3' instruction) which causes an exception that we will catch with our vectored exception handler.

    VirtualProtect(reinterpret_cast<LPVOID>(deserializeAddress), OVERWRITE_INSTR_SIZE, oldProtection, &oldProtection);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReasonForCall, LPVOID lpReserved) {
    switch (ulReasonForCall) {
    case DLL_PROCESS_ATTACH:
        std::thread(main).detach();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}