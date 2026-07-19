#include "DebugLog.h"

#include <stdarg.h>
#include <stdio.h>

namespace DebugLog
{
    void begin(uint32_t baud)
    {
        // ESP32-S3 boards vary: DevKitC often exposes UART0, while SuperMini
        // commonly uses native USB CDC for upload and monitor.
        Serial.begin(baud);
        Serial0.begin(baud);
    }

    void print(const char *message)
    {
        Serial.print(message);
        Serial0.print(message);
    }

    void println()
    {
        Serial.println();
        Serial0.println();
    }

    void println(const char *message)
    {
        Serial.println(message);
        Serial0.println(message);
    }

    void printf(const char *format, ...)
    {
        char buffer[256];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        Serial.print(buffer);
        Serial0.print(buffer);
    }
}
