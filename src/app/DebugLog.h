#pragma once

#include <Arduino.h>

namespace DebugLog
{
    void begin(uint32_t baud);
    void print(const char *message);
    void println();
    void println(const char *message);
    void printf(const char *format, ...);
}
