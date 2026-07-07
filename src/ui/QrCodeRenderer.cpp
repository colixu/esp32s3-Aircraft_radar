#include "QrCodeRenderer.h"

#include <string.h>

namespace
{
    constexpr uint8_t kQrVersion = 3;
    constexpr uint8_t kQrSize = 29;
    constexpr uint8_t kDataCodewords = 55;
    constexpr uint8_t kEccCodewords = 15;
    constexpr uint8_t kTotalCodewords = kDataCodewords + kEccCodewords;

    struct BitBuffer
    {
        uint8_t bytes[kDataCodewords] = {};
        uint16_t bitLength = 0;
    };

    bool modules[kQrSize][kQrSize];
    bool functions[kQrSize][kQrSize];

    uint8_t gfMultiply(uint8_t x, uint8_t y)
    {
        uint8_t z = 0;
        for (uint8_t i = 0; i < 8; ++i)
        {
            if ((y & 1) != 0)
            {
                z ^= x;
            }

            const bool carry = (x & 0x80) != 0;
            x <<= 1;
            if (carry)
            {
                x ^= 0x1D;
            }
            y >>= 1;
        }
        return z;
    }

    void reedSolomonDivisor(uint8_t *divisor, uint8_t degree)
    {
        memset(divisor, 0, degree);
        divisor[degree - 1] = 1;

        uint8_t root = 1;
        for (uint8_t i = 0; i < degree; ++i)
        {
            for (uint8_t j = 0; j < degree; ++j)
            {
                divisor[j] = gfMultiply(divisor[j], root);
                if (j + 1 < degree)
                {
                    divisor[j] ^= divisor[j + 1];
                }
            }
            root = gfMultiply(root, 0x02);
        }
    }

    void reedSolomonRemainder(const uint8_t *data, uint8_t dataLength, uint8_t *ecc, uint8_t eccLength)
    {
        uint8_t divisor[kEccCodewords];
        reedSolomonDivisor(divisor, eccLength);
        memset(ecc, 0, eccLength);

        for (uint8_t i = 0; i < dataLength; ++i)
        {
            const uint8_t factor = data[i] ^ ecc[0];
            memmove(&ecc[0], &ecc[1], eccLength - 1);
            ecc[eccLength - 1] = 0;

            for (uint8_t j = 0; j < eccLength; ++j)
            {
                ecc[j] ^= gfMultiply(divisor[j], factor);
            }
        }
    }

    void appendBits(BitBuffer &buffer, uint32_t value, uint8_t count)
    {
        for (int8_t i = count - 1; i >= 0; --i)
        {
            if (((value >> i) & 1U) != 0)
            {
                buffer.bytes[buffer.bitLength >> 3] |= 0x80 >> (buffer.bitLength & 7);
            }
            ++buffer.bitLength;
        }
    }

    bool buildDataCodewords(const char *payload, uint8_t *data)
    {
        BitBuffer buffer;
        const size_t length = strlen(payload);
        if (length > 53)
        {
            return false;
        }

        appendBits(buffer, 0x4, 4);
        appendBits(buffer, static_cast<uint32_t>(length), 8);
        for (size_t i = 0; i < length; ++i)
        {
            appendBits(buffer, static_cast<uint8_t>(payload[i]), 8);
        }

        const uint16_t capacityBits = kDataCodewords * 8;
        const uint8_t terminatorBits = min<uint16_t>(4, capacityBits - buffer.bitLength);
        appendBits(buffer, 0, terminatorBits);

        while ((buffer.bitLength & 7) != 0)
        {
            appendBits(buffer, 0, 1);
        }

        memcpy(data, buffer.bytes, kDataCodewords);
        for (uint8_t i = buffer.bitLength / 8; i < kDataCodewords; ++i)
        {
            data[i] = (i & 1) == 0 ? 0xEC : 0x11;
        }

        return true;
    }

    void clearMatrix()
    {
        memset(modules, 0, sizeof(modules));
        memset(functions, 0, sizeof(functions));
    }

    void setFunctionModule(int8_t x, int8_t y, bool dark)
    {
        if (x < 0 || y < 0 || x >= kQrSize || y >= kQrSize)
        {
            return;
        }

        modules[y][x] = dark;
        functions[y][x] = true;
    }

    void drawFinderPattern(int8_t x, int8_t y)
    {
        for (int8_t dy = -4; dy <= 4; ++dy)
        {
            for (int8_t dx = -4; dx <= 4; ++dx)
            {
                const int8_t xx = x + dx;
                const int8_t yy = y + dy;
                const int8_t dist = max(abs(dx), abs(dy));
                const bool dark = dist == 3 || dist <= 1;
                setFunctionModule(xx, yy, dark);
            }
        }
    }

    void drawAlignmentPattern(int8_t x, int8_t y)
    {
        for (int8_t dy = -2; dy <= 2; ++dy)
        {
            for (int8_t dx = -2; dx <= 2; ++dx)
            {
                const int8_t dist = max(abs(dx), abs(dy));
                setFunctionModule(x + dx, y + dy, dist == 2 || dist == 0);
            }
        }
    }

    void drawFunctionPatterns()
    {
        drawFinderPattern(3, 3);
        drawFinderPattern(kQrSize - 4, 3);
        drawFinderPattern(3, kQrSize - 4);
        drawAlignmentPattern(22, 22);

        for (uint8_t i = 8; i < kQrSize - 8; ++i)
        {
            setFunctionModule(i, 6, (i & 1) == 0);
            setFunctionModule(6, i, (i & 1) == 0);
        }

        setFunctionModule(8, kQrSize - 8, true);

        for (uint8_t i = 0; i <= 8; ++i)
        {
            if (i != 6)
            {
                setFunctionModule(8, i, false);
                setFunctionModule(i, 8, false);
            }
        }
        for (uint8_t i = 0; i < 8; ++i)
        {
            setFunctionModule(kQrSize - 1 - i, 8, false);
            setFunctionModule(8, kQrSize - 1 - i, false);
        }
    }

    uint16_t getFormatBits()
    {
        const uint8_t eclLow = 1;
        const uint8_t mask = 0;
        const uint16_t data = (eclLow << 3) | mask;
        uint16_t remainder = data << 10;

        for (int8_t i = 14; i >= 10; --i)
        {
            if (((remainder >> i) & 1U) != 0)
            {
                remainder ^= 0x537 << (i - 10);
            }
        }

        return static_cast<uint16_t>(((data << 10) | remainder) ^ 0x5412);
    }

    void drawFormatBits()
    {
        const uint16_t bits = getFormatBits();

        for (uint8_t i = 0; i <= 5; ++i)
        {
            setFunctionModule(8, i, ((bits >> i) & 1U) != 0);
        }
        setFunctionModule(8, 7, ((bits >> 6) & 1U) != 0);
        setFunctionModule(8, 8, ((bits >> 7) & 1U) != 0);
        setFunctionModule(7, 8, ((bits >> 8) & 1U) != 0);
        for (uint8_t i = 9; i < 15; ++i)
        {
            setFunctionModule(14 - i, 8, ((bits >> i) & 1U) != 0);
        }

        for (uint8_t i = 0; i < 8; ++i)
        {
            setFunctionModule(kQrSize - 1 - i, 8, ((bits >> i) & 1U) != 0);
        }
        for (uint8_t i = 8; i < 15; ++i)
        {
            setFunctionModule(8, kQrSize - 15 + i, ((bits >> i) & 1U) != 0);
        }
        setFunctionModule(8, kQrSize - 8, true);
    }

    bool dataBitAt(const uint8_t *codewords, uint16_t bitIndex)
    {
        return ((codewords[bitIndex >> 3] >> (7 - (bitIndex & 7))) & 1U) != 0;
    }

    void drawCodewords(const uint8_t *codewords)
    {
        uint16_t bitIndex = 0;
        const uint16_t bitCount = kTotalCodewords * 8;

        for (int8_t right = kQrSize - 1; right >= 1; right -= 2)
        {
            if (right == 6)
            {
                --right;
            }

            for (uint8_t vert = 0; vert < kQrSize; ++vert)
            {
                const int8_t y = ((right + 1) & 2) == 0 ? kQrSize - 1 - vert : vert;
                for (uint8_t j = 0; j < 2; ++j)
                {
                    const int8_t x = right - j;
                    if (functions[y][x])
                    {
                        continue;
                    }

                    bool dark = false;
                    if (bitIndex < bitCount)
                    {
                        dark = dataBitAt(codewords, bitIndex);
                        ++bitIndex;
                    }

                    const bool mask = ((x + y) & 1) == 0;
                    modules[y][x] = dark ^ mask;
                }
            }
        }
    }

    bool buildQrMatrix(const char *payload)
    {
        uint8_t codewords[kTotalCodewords] = {};
        if (!buildDataCodewords(payload, codewords))
        {
            return false;
        }

        reedSolomonRemainder(codewords, kDataCodewords, &codewords[kDataCodewords], kEccCodewords);

        clearMatrix();
        drawFunctionPatterns();
        drawCodewords(codewords);
        drawFormatBits();
        return true;
    }
}

bool QrCodeRenderer::drawWifiQr(TFT_eSprite &canvas,
                                const char *ssid,
                                const char *password,
                                int16_t centerX,
                                int16_t topY,
                                int16_t maxSize)
{
    if (ssid == nullptr || ssid[0] == '\0')
    {
        return false;
    }

    char payload[80];
    if (password != nullptr && password[0] != '\0')
    {
        snprintf(payload, sizeof(payload), "WIFI:T:WPA;S:%s;P:%s;;", ssid, password);
    }
    else
    {
        snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;;", ssid);
    }

    if (!buildQrMatrix(payload))
    {
        return false;
    }

    const int16_t quietModules = 4;
    const int16_t fullModules = kQrSize + quietModules * 2;
    const int16_t scale = max<int16_t>(2, maxSize / fullModules);
    const int16_t qrPixels = fullModules * scale;
    const int16_t left = centerX - qrPixels / 2;

    canvas.fillRect(left, topY, qrPixels, qrPixels, TFT_WHITE);

    for (uint8_t y = 0; y < kQrSize; ++y)
    {
        for (uint8_t x = 0; x < kQrSize; ++x)
        {
            if (modules[y][x])
            {
                canvas.fillRect(left + (x + quietModules) * scale,
                                topY + (y + quietModules) * scale,
                                scale,
                                scale,
                                TFT_BLACK);
            }
        }
    }

    return true;
}
