//
// Created by david on 10/27/2025.
//

// Global module fragment
module;


// Engine Exports
export module HOX.Logger;

// Other imports
import std;

import HOX.Win32;

export namespace HOX {
    // Logging severity
    enum Severity {
        Normal,
        Warning,
        Error,
        ErrorNoCrash,
        Info,
        Debug
    };


    class Logger {
    public:
        Logger() = delete;
        ~Logger() = delete;

        // Prevent copy and move
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) = delete;
        Logger& operator=(Logger&&) = delete;

        static void LogMessage(const Severity& MessageSeverity, const std::string& Message, HOX::Win32::DWORD ErrorCode = HOX::Win32::GetLastError_(), const std::source_location& Location = std::source_location::current());

    };
}


