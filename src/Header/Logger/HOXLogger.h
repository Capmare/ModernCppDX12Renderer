//
// Created by david on 10/27/2025.
//

#ifndef MODERNCPPDX12RENDERER_HOXLOGGER_H
#define MODERNCPPDX12RENDERER_HOXLOGGER_H
#include <source_location>
#include <string>

enum HOXSeverity {
    Normal,
    Warning,
    Error,
    ErrorNoCrash,
    Info,
    Debug
};



class HOXLogger {
    public:
    HOXLogger() = delete;
    ~HOXLogger() = delete;

    // Prevent copy and move
    HOXLogger(const HOXLogger&) = delete;
    HOXLogger& operator=(const HOXLogger&) = delete;
    HOXLogger(HOXLogger&&) = delete;
    HOXLogger& operator=(HOXLogger&&) = delete;

    static void Message(const HOXSeverity& Severity, const std::string& Message, const std::source_location& Location = std::source_location::current());

};


#endif //MODERNCPPDX12RENDERER_HOXLOGGER_H