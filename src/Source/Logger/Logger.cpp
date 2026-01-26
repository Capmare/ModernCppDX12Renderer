//
// Created by david on 10/27/2025.
//

module;
#include <windows.h>


module HOX.Logger;
import std;

namespace HOX {
   void Logger::LogMessage(const Severity &MessageSeverity, const std::string &Message,
                            DWORD ErrorCode, const std::source_location &Location) {
        std::string SeverityStr;
        std::string Color;

        switch (MessageSeverity) {
            case Severity::Normal: SeverityStr = "NORMAL";
                std::cout << Color << SeverityStr << "\033[0m"
                          << "\t" << Message << std::endl;
                return;
            case Severity::ErrorNoCrash:
            case Severity::Error: SeverityStr = "ERROR";
                Color = "\033[31m";
                break;
            case Severity::Warning: SeverityStr = "WARNING";
                Color = "\033[33m";
                break;
            case Severity::Info: SeverityStr = "INFO";
                Color = "\033[36m";
                break;
            case Severity::Debug: SeverityStr = "DEBUG";
                Color = "\033[32m";
                break;
            default: SeverityStr = "UNKNOWN";
                Color = "\033[0m";
                break;
        }

        std::wstring ErrorMessageW;
        if ((MessageSeverity == Severity::Error || MessageSeverity == Severity::ErrorNoCrash) && ErrorCode != 0) {
            LPWSTR ErrorBuffer = nullptr;
            FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                ErrorCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPWSTR)&ErrorBuffer,
                0,
                nullptr
            );

            ErrorMessageW = ErrorBuffer ? ErrorBuffer : L"Unknown error.";
            LocalFree(ErrorBuffer);
        }

        // Convert wide string to narrow for console output
        std::wstring_convert<std::codecvt_utf8<wchar_t>> Converter;
        std::string ErrorMessage = Converter.to_bytes(ErrorMessageW);

        std::cout << Color << SeverityStr << "\033[0m"
                  << "\t" << Message
                  << "\t" << Location.file_name()
                  << ":" << Location.line()
                  << "\t" << Location.function_name();

        if (!ErrorMessage.empty())
            std::cout << "\t\033[31m" << ErrorMessage;

        std::cout << std::endl;

        if (MessageSeverity == Severity::Error) {
            throw std::runtime_error(std::format(
                "Message:\t{}\nLine:\t{}:{}\nIn function:\t{}\nWindowsError:\t{}",
                Message,
                Location.file_name(),
                Location.line(),
                Location.function_name(),
                ErrorMessage
            ));

        }
    }
}

