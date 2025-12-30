#include "pch.h"
#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <memory>
#include "Header/Window/Window.h"
#include "Header/Window/WindowBuilder.h"

// Initialize console and redirect standard streams
void InitConsole() {
    if (AllocConsole()) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);

        std::cout.clear();
        std::cerr.clear();
        std::clog.clear();

        std::cout << "Console initialized.\n";
    }
}

// Map common SEH exception codes to strings
const char* GetExceptionString(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:       return "Access Violation";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:  return "Array Bounds Exceeded";
        case EXCEPTION_BREAKPOINT:             return "Breakpoint";
        case EXCEPTION_DATATYPE_MISALIGNMENT:  return "Datatype Misalignment";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:     return "Floating-Point Divide by Zero";
        case EXCEPTION_ILLEGAL_INSTRUCTION:    return "Illegal Instruction";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:     return "Integer Divide by Zero";
        case EXCEPTION_STACK_OVERFLOW:         return "Stack Overflow";
        default:                               return "Unknown Exception";
    }
}


void PrintStackTrace(EXCEPTION_POINTERS* pExceptionInfo) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return;

    // Initialize DbgHelp
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);

    CONTEXT context = *pExceptionInfo->ContextRecord;
    STACKFRAME64 stackFrame;
    ZeroMemory(&stackFrame, sizeof(STACKFRAME64));

#ifdef _M_X64
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#else
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#endif

    for (int i = 0; i < 62; i++) {
        if (!StackWalk64(machineType, GetCurrentProcess(), GetCurrentThread(),
                         &stackFrame, &context, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;

        if (stackFrame.AddrPC.Offset == 0)
            break;

        char symbolBuffer[sizeof(SYMBOL_INFO) + 256] = {0};
        PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuffer;
        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSymbol->MaxNameLen = 255;

        DWORD64 displacement = 0;
        if (SymFromAddr(GetCurrentProcess(), stackFrame.AddrPC.Offset, &displacement, pSymbol)) {
            std::cout << i << ": " << pSymbol->Name << " - 0x" << std::hex << pSymbol->Address << std::dec << "\n";
        } else {
            std::cout << i << ": ??? - 0x" << std::hex << stackFrame.AddrPC.Offset << std::dec << "\n";
        }
    }

    SymCleanup(GetCurrentProcess());
}


// Crash handler for unhandled exceptions
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* pExceptionInfo) {
    // Ensure console exists
    AllocConsole();
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return EXCEPTION_EXECUTE_HANDLER;

    DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
    void* addr = pExceptionInfo->ExceptionRecord->ExceptionAddress;
    DWORD written;

    // Set console text color to red for errors
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);

    char buffer[512];
    snprintf(buffer, sizeof(buffer),
             "Unhandled exception occurred!\n"
             "Exception Code: 0x%08X (%s)\n"
             "Faulting Address: %p\n"
             "Press Enter to exit...\n",
             code, GetExceptionString(code), addr);

    WriteConsoleA(hConsole, buffer, (DWORD)strlen(buffer), &written, nullptr);

    // Reset console text color
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    PrintStackTrace(pExceptionInfo);


    // Wait for Enter key
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput != INVALID_HANDLE_VALUE) {
        char buf[2];
        DWORD read;
        ReadConsoleA(hInput, buf, 2, &read, nullptr);
    }


    return EXCEPTION_EXECUTE_HANDLER;
}


int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

#if not _DEBUG
    InitConsole();
    SetUnhandledExceptionFilter(CrashHandler);
#endif


    try {
        std::unique_ptr<HOX::Window> Window = std::make_unique<HOX::Window>(hInstance, nCmdShow);
        Window->SetWindowLocationAndDimension({300, 300, 1920, 1080});
        Window->Run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal Error: " << e.what() << "\n";
        std::cerr << "Press Enter to exit...\n";
        std::cin.get();
        return -1;
    } catch (...) {
        std::cerr << "Unknown fatal error.\n";
        std::cerr << "Press Enter to exit...\n";
        std::cin.get();
        return -1;
    }

    return 0;
}
