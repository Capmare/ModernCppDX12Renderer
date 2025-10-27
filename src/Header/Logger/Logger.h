//
// Created by david on 10/27/2025.
//

#ifndef MODERNCPPDX12RENDERER_HOXLOGGER_H
#define MODERNCPPDX12RENDERER_HOXLOGGER_H
#include <source_location>
#include <string>

namespace HOX {
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

        static void LogMessage(const Severity& MessageSeverity, const std::string& Message, const std::source_location& Location = std::source_location::current());

    };
}


#endif //MODERNCPPDX12RENDERER_HOXLOGGER_H