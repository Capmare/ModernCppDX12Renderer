import std;
import HOX.Win32;
import HOX.Window;

static void InitConsole()
{
    using namespace HOX::Win32;

    if (AllocConsole_())
    {
        FILE* fp{};
        Reopen(&fp, "CONOUT$", "w", Out());
        Reopen(&fp, "CONOUT$", "w", Err());
        Reopen(&fp, "CONIN$",  "r", In());

        std::cout.clear();
        std::cerr.clear();
        std::clog.clear();
        std::cout << "Console initialized.\n";
    }
}

static const char* GetExceptionString(HOX::Win32::DWORD code)
{
    using namespace HOX::Win32;

    switch (code)
    {
        case ExAccessViolation:      return "Access Violation";
        case ExArrayBoundsExceeded:  return "Array Bounds Exceeded";
        case ExBreakpoint:           return "Breakpoint";
        case ExDatatypeMisalignment: return "Datatype Misalignment";
        case ExFltDivideByZero:      return "Floating-Point Divide by Zero";
        case ExIllegalInstruction:   return "Illegal Instruction";
        case ExIntDivideByZero:      return "Integer Divide by Zero";
        case ExStackOverflow:        return "Stack Overflow";
        default:                     return "Unknown Exception";
    }
}

static void PrintStackTrace(HOX::Win32::EXCEPTION_POINTERS* p)
{
    using namespace HOX::Win32;

    HANDLE hConsole = GetStdHandle_(StdOutputHandle);
    if (hConsole == InvalidHandleValue()) return;

    SymInitialize_(CurrentProcess());

    CONTEXT ctx = *p->ContextRecord;

    STACKFRAME64 frame{};
    ZeroMem(&frame, sizeof(frame));

#ifdef _M_X64
    DWORD machine = MachineAmd64;
    frame.AddrPC.Offset    = ctx.Rip;
    frame.AddrPC.Mode      = AddrModeFlatMode;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode   = AddrModeFlatMode;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrStack.Mode   = AddrModeFlatMode;
#else
    DWORD machine = MachineI386;
    frame.AddrPC.Offset    = ctx.Eip;
    frame.AddrPC.Mode      = AddrModeFlatMode;
    frame.AddrFrame.Offset = ctx.Ebp;
    frame.AddrFrame.Mode   = AddrModeFlatMode;
    frame.AddrStack.Offset = ctx.Esp;
    frame.AddrStack.Mode   = AddrModeFlatMode;
#endif

    for (int i = 0; i < 62; ++i)
    {
        if (!StackWalk64_(machine, CurrentProcess(), CurrentThread(), &frame, &ctx))
            break;
        if (frame.AddrPC.Offset == 0)
            break;

        char buf[sizeof(SYMBOL_INFO) + 256]{};
        auto* sym = reinterpret_cast<PSYMBOL_INFO>(buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen   = 255;

        DWORD64 disp{};
        if (SymFromAddr_(CurrentProcess(), frame.AddrPC.Offset, &disp, sym))
            std::cout << i << ": " << sym->Name << " - 0x" << std::hex << sym->Address << std::dec << "\n";
        else
            std::cout << i << ": ??? - 0x" << std::hex << frame.AddrPC.Offset << std::dec << "\n";
    }

    SymCleanup_(CurrentProcess());
}

static HOX::Win32::LONG __stdcall CrashHandler(HOX::Win32::EXCEPTION_POINTERS* p)
{
    using namespace HOX::Win32;

    AllocConsole_();

    HANDLE hConsole = GetStdHandle_(StdOutputHandle);
    if (hConsole == InvalidHandleValue()) return ExExecuteHandler;

    DWORD code = p->ExceptionRecord->ExceptionCode;
    void* addr = p->ExceptionRecord->ExceptionAddress;

    SetConsoleColor(hConsole, FgRed | FgIntensity);

    char msg[512]{};
    SNPrintf(msg, sizeof(msg),
        "Unhandled exception occurred!\n"
        "Exception Code: 0x%08X (%s)\n"
        "Faulting Address: %p\n"
        "Press Enter to exit...\n",
        code, GetExceptionString(code), addr);

    DWORD written{};
    WriteConsoleA_(hConsole, msg, (DWORD)StrLen(msg), &written);

    SetConsoleColor(hConsole, FgRed | FgGreen | FgBlue);

    PrintStackTrace(p);

    HANDLE hInput = GetStdHandle_(StdInputHandle);
    if (hInput != InvalidHandleValue())
    {
        char tmp[2]{};
        DWORD read{};
        ReadConsoleA_(hInput, tmp, 2, &read);
    }

    return ExExecuteHandler;
}

int __stdcall WinMain(HOX::Win32::HINSTANCE hInstance, HOX::Win32::HINSTANCE, char*, int nCmdShow)
{
#if !defined(_DEBUG)
    InitConsole();
    HOX::Win32::SetUnhandledExceptionFilter_((HOX::Win32::LPTOP_LEVEL_EXCEPTION_FILTER)&CrashHandler);
#endif

    try
    {
        auto window = std::make_unique<HOX::Window>(hInstance, nCmdShow);
        window->SetWindowLocationAndDimension({300, 300, 1920, 1080});
        window->Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal Error: " << e.what() << "\nPress Enter...\n";
        std::cin.get();
        return -1;
    }
    catch (...)
    {
        std::cerr << "Unknown fatal error.\nPress Enter...\n";
        std::cin.get();
        return -1;
    }

    return 0;
}
