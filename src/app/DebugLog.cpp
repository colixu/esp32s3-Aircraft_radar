#include "DebugLog.h"

#include <stdarg.h>
#include <stdio.h>

#include "FeatureFlags.h"

namespace DebugLog
{
    namespace
    {
        bool enabled_ = false;
    }

    void begin(uint32_t baud)
    {
#if ENABLE_SERIAL_IO
        // ESP32-S3 boards vary: DevKitC often exposes UART0, while SuperMini
        // commonly uses native USB CDC for upload and monitor.
        Serial.begin(baud);
        Serial0.begin(baud);
#else
        (void)baud;
#endif
    }

    void setEnabled(bool enabled)
    {
        enabled_ = enabled;
    }

    bool isEnabled()
    {
        return enabled_;
    }

    void print(const char *message)
    {
        if (!enabled_)
        {
            return;
        }
#if ENABLE_SERIAL_IO
        Serial.print(message);
        Serial0.print(message);
#else
        (void)message;
#endif
    }

    void println()
    {
        if (!enabled_)
        {
            return;
        }
#if ENABLE_SERIAL_IO
        Serial.println();
        Serial0.println();
#endif
    }

    void println(const char *message)
    {
        if (!enabled_)
        {
            return;
        }
#if ENABLE_SERIAL_IO
        Serial.println(message);
        Serial0.println(message);
#else
        (void)message;
#endif
    }

    void printf(const char *format, ...)
    {
        if (!enabled_)
        {
            return;
        }
#if ENABLE_SERIAL_IO
        char buffer[256];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        Serial.print(buffer);
        Serial0.print(buffer);
#else
        (void)format;
#endif
    }
}
