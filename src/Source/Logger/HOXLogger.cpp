//
// Created by david on 10/27/2025.
//

#include "../../Header/Logger/HOXLogger.h"

#include <format>
#include <iostream>
#include <windows.h>

void HOXLogger::Message(const HOXSeverity &Severity, const std::string &Message, const std::source_location &Location) {
    std::string SeverityStr;
    std::string Color;

    switch (Severity) {
        case HOXSeverity::Normal: SeverityStr = "NORMAL";
            std::cout << Color << SeverityStr << "\033[0m"
            << "\t" << Message << std::endl; return;
            break;
        case HOXSeverity::ErrorNoCrash:
        case HOXSeverity::Error: SeverityStr = "ERROR";
            Color = "\033[31m";
            break;
        case HOXSeverity::Warning: SeverityStr = "WARNING";
            Color = "\033[33m";
            break;
        case HOXSeverity::Info: SeverityStr = "INFO";
            Color = "\033[36m";
            break;
        case HOXSeverity::Debug: SeverityStr = "DEBUG";
            Color = "\033[32m";
            break;
        default: SeverityStr = "UNKNOWN";
            Color = "\033[0m";
            break;
    }

    std::cout << Color << SeverityStr << "\033[0m"
            << "\t" << Message
            << "\t" << Location.file_name()
            << ":" << Location.line()
            << "\t" << Location.function_name()
            << std::endl;

    if (Severity == HOXSeverity::Error) {
        throw std::runtime_error(std::format(
            "Message:\t{}\nLine:\t{}:{}\nIn function:\t{}",
            Message,
            Location.file_name(),
            Location.line(),
            Location.function_name()
        ));
    }
}
