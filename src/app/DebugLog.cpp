#include "DebugLog.h"

#include <stdarg.h>
#include <stdio.h>

namespace DebugLog
{
    void begin(uint32_t baud)
    {
        // Use the UART interface that also carries ESP-ROM boot logs and upload traffic.
        Serial0.begin(baud);
    }

    void print(const char *message)
    {
        Serial0.print(message);
    }

    void println()
    {
        Serial0.println();
    }

    void println(const char *message)
    {
        Serial0.println(message);
    }

    void printf(const char *format, ...)
    {
        char buffer[256];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        Serial0.print(buffer);
    }
}
