//
// Created by david on 10/27/2025.
//



module HOX.Logger;
import std;
import HOX.Win32;

namespace HOX {
   void Logger::LogMessage(const Severity &MessageSeverity, const std::string &Message,
                            HOX::Win32::DWORD ErrorCode, const std::source_location &Location) {
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
            HOX::Win32::LPWSTR ErrorBuffer = nullptr;
            HOX::Win32::FormatMessageW_(
                HOX::Win32::FormatMessageAllocateBuffer | HOX::Win32::FormatMessageFromSystem | HOX::Win32::FormatMessageIgnoreInsters,
                nullptr,
                ErrorCode,
                HOX::Win32::MakeLangId_(HOX::Win32::LanguageNeutral, HOX::Win32::SublanguageDefault),
                (HOX::Win32::LPWSTR)&ErrorBuffer,
                0,
                nullptr
            );

            ErrorMessageW = ErrorBuffer ? ErrorBuffer : L"Unknown error.";
            HOX::Win32::LocalFree_(ErrorBuffer);
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

