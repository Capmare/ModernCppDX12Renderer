module;


#include <Windows.h>
#include <DbgHelp.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <combaseapi.h>
#include <wrl/client.h>
#include <windowsx.h>

export module HOX.Win32;


export namespace HOX::Win32 {
    using Microsoft::WRL::ComPtr;

    // Types
    using ::HINSTANCE;
    using ::HWND;
    using ::HANDLE;
    using ::DWORD;
    using ::UINT;
    using ::WPARAM;
    using ::LPARAM;
    using ::LRESULT;
    using ::LONG;
    using ::LONG_PTR;
    using ::BOOL;
    using ::WORD;
    using ::WNDPROC;
    using ::WNDCLASS;
    using ::LPCSTR;
    using ::LPCWSTR;
    using ::LPWSTR;
    using ::WNDCLASSW;

    using ::MSG;
    using ::RECT;

    using ::CONTEXT;
    using ::EXCEPTION_POINTERS;
    using ::LPTOP_LEVEL_EXCEPTION_FILTER;

    using ::STACKFRAME64;
    using ::SYMBOL_INFO;
    using ::PSYMBOL_INFO;
    using ::DWORD64;

    using u32 = std::uint32_t;

    // C runtime (avoid ::stdout macro issues)
    using FILE = ::FILE;

    inline FILE *Out() noexcept { return stdout; }
    inline FILE *Err() noexcept { return stderr; }
    inline FILE *In() noexcept { return stdin; }

    inline errno_t Reopen(FILE **fp, const char *file, const char *mode, FILE *stream) noexcept {
        return ::freopen_s(fp, file, mode, stream);
    }

    inline size_t StrLen(const char *s) noexcept { return ::strlen(s); }

    inline int SNPrintf(char *dst, size_t dstSize, const char *fmt, ...) noexcept {
        va_list args;
        va_start(args, fmt);
        int r = ::vsnprintf_s(dst, dstSize, _TRUNCATE, fmt, args);
        va_end(args);
        return r;
    }

    // Constants
    inline constexpr DWORD StdOutputHandle = STD_OUTPUT_HANDLE;
    inline constexpr DWORD StdErrorHandle = STD_ERROR_HANDLE;
    inline constexpr DWORD StdInputHandle = STD_INPUT_HANDLE;

    inline constexpr UINT MsgDestroy = WM_DESTROY;
    inline constexpr UINT MsgPaint = WM_PAINT;
    inline constexpr UINT MsgSize = WM_SIZE;
    inline constexpr UINT MsgQuit = WM_QUIT;

    inline constexpr UINT PMRemove = PM_REMOVE;

    inline constexpr DWORD WSOverlappedWindow = WS_OVERLAPPEDWINDOW;
    inline constexpr DWORD WSOverlappedDefault = WS_OVERLAPPED;
    inline constexpr INT CWUseDefault = CW_USEDEFAULT;

    inline constexpr int GWLPUserData = GWLP_USERDATA;

    // exception filter return value
    inline constexpr LONG ExExecuteHandler = EXCEPTION_EXECUTE_HANDLER;

    // exception codes (exported values; don’t use EXCEPTION_* in consumers)
    inline constexpr DWORD ExAccessViolation = EXCEPTION_ACCESS_VIOLATION;
    inline constexpr DWORD ExArrayBoundsExceeded = EXCEPTION_ARRAY_BOUNDS_EXCEEDED;
    inline constexpr DWORD ExBreakpoint = EXCEPTION_BREAKPOINT;
    inline constexpr DWORD ExDatatypeMisalignment = EXCEPTION_DATATYPE_MISALIGNMENT;
    inline constexpr DWORD ExFltDivideByZero = EXCEPTION_FLT_DIVIDE_BY_ZERO;
    inline constexpr DWORD ExIllegalInstruction = EXCEPTION_ILLEGAL_INSTRUCTION;
    inline constexpr DWORD ExIntDivideByZero = EXCEPTION_INT_DIVIDE_BY_ZERO;
    inline constexpr DWORD ExStackOverflow = EXCEPTION_STACK_OVERFLOW;

    // Message Formating
    inline constexpr DWORD FormatMessageAllocateBuffer = FORMAT_MESSAGE_ALLOCATE_BUFFER;
    inline constexpr DWORD FormatMessageFromSystem = FORMAT_MESSAGE_FROM_SYSTEM;
    inline constexpr DWORD FormatMessageIgnoreInsters = FORMAT_MESSAGE_IGNORE_INSERTS;

    // console colors
    inline constexpr WORD FgRed = FOREGROUND_RED;
    inline constexpr WORD FgGreen = FOREGROUND_GREEN;
    inline constexpr WORD FgBlue = FOREGROUND_BLUE;
    inline constexpr WORD FgIntensity = FOREGROUND_INTENSITY;

    // machine types
    inline constexpr DWORD MachineAmd64 = IMAGE_FILE_MACHINE_AMD64;
    inline constexpr DWORD MachineI386 = IMAGE_FILE_MACHINE_I386;

    // Language types
    inline constexpr USHORT LanguageNeutral = LANG_NEUTRAL;
    inline constexpr USHORT SublanguageDefault = SUBLANG_DEFAULT;

    // Drawing types
    inline constexpr UINT CsHRedraw = CS_HREDRAW;
    inline constexpr UINT CsVRedraw = CS_VREDRAW;

    // dbghelp address mode (real enum value)
    inline constexpr ADDRESS_MODE AddrModeFlatMode = AddrModeFlat;

    // pointer “constants” -> functions (avoid constexpr issues)
    inline HANDLE InvalidHandleValue() noexcept { return INVALID_HANDLE_VALUE; }

    // Win32 wrappers (safe names)
    inline BOOL AllocConsole_() noexcept { return ::AllocConsole(); }
    inline HANDLE GetStdHandle_(DWORD which) noexcept { return ::GetStdHandle(which); }

    // GetCurrentProcess/GetCurrentThread are macros -> call without ::
    inline HANDLE CurrentProcess() noexcept { return GetCurrentProcess(); }
    inline HANDLE CurrentThread() noexcept { return GetCurrentThread(); }

    inline void ZeroMem(void *p, size_t n) noexcept { ::ZeroMemory(p, n); }

    inline int GetXLParam(LPARAM lp) { return GET_X_LPARAM(lp); }
    inline int GetYLParam(LPARAM lp) { return GET_Y_LPARAM(lp); }

    inline BOOL SetConsoleColor(HANDLE h, WORD attr) noexcept {
        return ::SetConsoleTextAttribute(h, attr);
    }

    inline BOOL WriteConsoleA_(HANDLE h, const char *s, DWORD n, DWORD *written) noexcept {
        return ::WriteConsoleA(h, s, n, written, nullptr);
    }

    inline BOOL ReadConsoleA_(HANDLE h, char *buf, DWORD n, DWORD *read) noexcept {
        return ::ReadConsoleA(h, buf, n, read, nullptr);
    }

    inline void SetUnhandledExceptionFilter_(LPTOP_LEVEL_EXCEPTION_FILTER f) noexcept {
        ::SetUnhandledExceptionFilter(f);
    }

    inline HANDLE CreateEvent_(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState,
                               LPCSTR lpName) {
        return ::CreateEvent(lpEventAttributes, bManualReset, bInitialState, lpName);
    }

    // window/message wrappers
    inline void ShowWindow_(HWND hwnd, int cmdShow) noexcept { ::ShowWindow(hwnd, cmdShow); }
    inline void PostQuitMessage_(int code) noexcept { ::PostQuitMessage(code); }

    inline WORD MakeLangId_(USHORT p, USHORT s) {
        return MAKELANGID(p, s);
    }

    inline DWORD GetLastError_() noexcept {
        return ::GetLastError();
    }

    inline BOOL GetClientRect_(HWND hwnd, RECT *rc) noexcept { return ::GetClientRect(hwnd, rc); }

    inline LONG_PTR SetWindowLongPtrW_(HWND hwnd, int idx, LONG_PTR v) noexcept {
        return ::SetWindowLongPtrW(hwnd, idx, v);
    }

    inline LONG_PTR GetWindowLongPtrW_(HWND hwnd, int idx) noexcept {
        return ::GetWindowLongPtrW(hwnd, idx);
    }

    inline BOOL PeekMessageW_(MSG *msg, HWND hwnd, UINT fmin, UINT fmax, UINT remove) noexcept {
        return ::PeekMessageW(msg, hwnd, fmin, fmax, remove);
    }

    inline BOOL TranslateMessage_(const MSG *msg) noexcept { return ::TranslateMessage(msg); }
    inline LRESULT DispatchMessageW_(const MSG *msg) noexcept { return ::DispatchMessageW(msg); }

    inline LRESULT DefWindowProcW_(HWND hwnd, UINT uMsg, WPARAM w, LPARAM l) noexcept {
        return ::DefWindowProcW(hwnd, uMsg, w, l);
    }

    inline HLOCAL LocalFree_(_Frees_ptr_opt_ HLOCAL hMem) {
        return LocalFree(hMem);
    }

    // dbghelp wrappers
    inline BOOL SymInitialize_(HANDLE proc) noexcept { return ::SymInitialize(proc, nullptr, TRUE); }
    inline BOOL SymCleanup_(HANDLE proc) noexcept { return ::SymCleanup(proc); }

    inline BOOL StackWalk64_(
        DWORD machineType,
        HANDLE hProcess,
        HANDLE hThread,
        STACKFRAME64 *frame,
        CONTEXT *ctx) noexcept {
        return ::StackWalk64(machineType, hProcess, hThread, frame, ctx,
                             nullptr, ::SymFunctionTableAccess64, ::SymGetModuleBase64, nullptr);
    }

    inline BOOL SymFromAddr_(
        HANDLE hProcess,
        DWORD64 addr,
        DWORD64 *disp,
        PSYMBOL_INFO sym) noexcept {
        return ::SymFromAddr(hProcess, addr, disp, sym);
    }

    inline DWORD FormatMessageW_(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId,
                                 LPWSTR lpBuffer, DWORD dwSize, va_list *Arguments) noexcept {
        return ::FormatMessageW(dwFlags, lpSource, dwMessageId, dwLanguageId, lpBuffer, dwSize, Arguments);
    }

    inline HWND CreateWindowExW_(
        _In_ DWORD dwExStyle,
        _In_opt_ LPCWSTR lpClassName,
        _In_opt_ LPCWSTR lpWindowName,
        _In_ DWORD dwStyle,
        _In_ int X,
        _In_ int Y,
        _In_ int nWidth,
        _In_ int nHeight,
        _In_opt_ HWND hWndParent,
        _In_opt_ HMENU hMenu,
        _In_opt_ HINSTANCE hInstance,
        _In_opt_ LPVOID lpParam) {
        return ::CreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent,
                                 hMenu, hInstance, lpParam);
    }

    inline ATOM RegisterClassW_( const WNDCLASSW *lpWndClass ) {
        return ::RegisterClassW(lpWndClass);
    }

    [[nodiscard]] constexpr bool Succeeded(HRESULT hr) noexcept { return hr >= 0; }
    [[nodiscard]] constexpr bool Failed(HRESULT hr) noexcept { return hr < 0; }


    // Replacement for the "IID" part of IID_PPV_ARGS(T)
    template<class T>
    [[nodiscard]] constexpr REFIID UuidOf() noexcept {
        return __uuidof(T);
    }

    // Replacement for the "PPV" part (void**) of IID_PPV_ARGS(...)
    template<class T>
    [[nodiscard]] constexpr void **PpvArgs(T **pp) noexcept {
        return reinterpret_cast<void **>(pp);
    }

    // Convenience overload for WRL ComPtr
    template<class T>
    [[nodiscard]] constexpr void **PpvArgs(ComPtr<T> &p) noexcept {
        return reinterpret_cast<void **>(p.ReleaseAndGetAddressOf());
    }
}
