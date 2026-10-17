#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_system.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// 4x4 Ordered Bayer Dithering Matrix (0..15 thresholds scaled to 0..255)
static const uint8_t BAYER4X4[4][4] = {
    {  15, 143,  47, 175 },
    { 207,  79, 239, 111 },
    {  63, 191,  31, 159 },
    { 255, 127, 223,  95 }
};

class SSD1306Fast {
public:
    static constexpr int WIDTH = 128;
    static constexpr int HEIGHT = 64;
    static constexpr int BUF_SIZE = (WIDTH * HEIGHT) / 8;

    uint8_t buffer[BUF_SIZE];
    uint8_t i2cAddr = 0x3C;
    int activeSda = 22;
    int activeScl = 23;
    bool connected = false;
    uint8_t lastContrast = 255;
    bool lastInvert = false;
    bool lastPower = true;

    bool probeBus(int sda, int scl, uint32_t speedHz, uint8_t* foundAddr) {
        i2c_driver_delete(I2C_NUM_0);
        gpio_reset_pin((gpio_num_t)sda);
        gpio_reset_pin((gpio_num_t)scl);

        i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = (gpio_num_t)sda;
        conf.scl_io_num = (gpio_num_t)scl;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = speedHz;
        if (i2c_param_config(I2C_NUM_0, &conf) != ESP_OK) return false;
        if (i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0) != ESP_OK) return false;

        const uint8_t pref[] = {0x3C, 0x3D};
        for (size_t i = 0; i < sizeof(pref); i++) {
            uint8_t addr = pref[i];
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_stop(cmd);
            esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(25));
            i2c_cmd_link_delete(cmd);
            if (err == ESP_OK) {
                *foundAddr = addr;
                return true;
            }
        }
        return false;
    }

    bool begin(int sdaPin = 22, int sclPin = 23, uint32_t clockHz = 400000) {
        activeSda = sdaPin;
        activeScl = sclPin;

        {
            i2c_config_t conf = {};
            conf.mode = I2C_MODE_MASTER;
            conf.sda_io_num = (gpio_num_t)sdaPin;
            conf.scl_io_num = (gpio_num_t)sclPin;
            conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
            conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
            conf.master.clk_speed = 100000;
            i2c_param_config(I2C_NUM_0, &conf);
            i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
        }

        struct PinPair { int sda; int scl; };
        const PinPair pairs[] = {
            {22, 23}, {23, 22},
            {21, 22}, {22, 21},
            {4, 15},  {15, 4},
            {5, 4},   {4, 5},
            {18, 19}, {19, 18},
            {25, 26}, {26, 25},
            {32, 33}, {33, 32}
        };

        uint8_t found = 0x3C;
        for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
            if (probeBus(pairs[i].sda, pairs[i].scl, 100000, &found)) {
                activeSda = pairs[i].sda;
                activeScl = pairs[i].scl;
                i2cAddr = found;
                connected = true;
                printf("[I2C-SCAN] Found SSD1306 at 0x%02X on SDA=%d, SCL=%d!\n",
                       i2cAddr, activeSda, activeScl);
                break;
            }
        }

        if (!connected) {
            activeSda = sdaPin;
            activeScl = sclPin;
            probeBus(activeSda, activeScl, 400000, &found);
        } else {
            uint8_t dummy = 0;
            if (!probeBus(activeSda, activeScl, clockHz, &dummy)) {
                probeBus(activeSda, activeScl, 100000, &dummy);
            }
        }

        static const uint8_t initSeq[] = {
            0xAE,       // Display OFF
            0xD5, 0xF0, // Max oscillator frequency
            0xA8, 0x3F, // Multiplex ratio 1/64
            0xD3, 0x00, // Display offset 0
            0x40,       // Start line 0
            0x8D, 0x14, // Charge pump ON
            0x20, 0x00, // Horizontal addressing mode
            0xA1,       // Segment re-map (col 127 -> SEG0)
            0xC8,       // COM output scan direction remapped
            0xDA, 0x12, // COM pins hardware config for 128x64
            0x81, 0xFF, // Max contrast
            0xD9, 0xF1, // Pre-charge period
            0xDB, 0x40, // VCOMH deselect level
            0xA4,       // Entire display ON from RAM
            0xA6,       // Normal display
            0xAF        // Display ON
        };

        for (size_t i = 0; i < sizeof(initSeq); i++) {
            sendCmd(initSeq[i]);
        }

        clear();
        display();
        return connected;
    }

    void sendCmd(uint8_t c) {
        uint8_t pkt[2] = {0x00, c};
        i2c_master_write_to_device(I2C_NUM_0, i2cAddr, pkt, 2, pdMS_TO_TICKS(20));
    }

    void setInvert(bool inv) {
        if (inv != lastInvert) {
            lastInvert = inv;
            sendCmd(inv ? 0xA7 : 0xA6);
        }
    }

    void setPower(bool on) {
        if (on != lastPower) {
            lastPower = on;
            sendCmd(on ? 0xAF : 0xAE);
        }
    }

    void setContrast(uint8_t bri) {
        if (bri != lastContrast) {
            lastContrast = bri;
            sendCmd(0x81);
            sendCmd(bri);
        }
    }

    inline void clear() {
        memset(buffer, 0, BUF_SIZE);
    }

    inline void setPixel(int x, int y, bool on = true) {
        if ((unsigned)x >= WIDTH || (unsigned)y >= HEIGHT) return;
        uint16_t idx = x + (y >> 3) * WIDTH;
        uint8_t mask = 1 << (y & 7);
        if (on) buffer[idx] |= mask;
        else    buffer[idx] &= ~mask;
    }

    // 4x4 Ordered Bayer Dithered pixel write (gray: 0..255)
    inline void setPixelGray(int x, int y, uint8_t gray) {
        if ((unsigned)x >= WIDTH || (unsigned)y >= HEIGHT) return;
        if (gray == 0) return;
        bool on = (gray >= BAYER4X4[y & 3][x & 3]);
        if (on) {
            buffer[x + (y >> 3) * WIDTH] |= (1 << (y & 7));
        }
    }

    inline void xorPixel(int x, int y) {
        if ((unsigned)x >= WIDTH || (unsigned)y >= HEIGHT) return;
        buffer[x + (y >> 3) * WIDTH] ^= (1 << (y & 7));
    }

    void drawHLine(int x0, int x1, int y, int step = 1) {
        if ((unsigned)y >= HEIGHT) return;
        if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
        if (x0 < 0) x0 = 0;
        if (x1 >= WIDTH) x1 = WIDTH - 1;
        for (int x = x0; x <= x1; x += step) {
            setPixel(x, y, true);
        }
    }

    void drawVLine(int x, int y0, int y1, bool on = true) {
        if ((unsigned)x >= WIDTH) return;
        if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
        if (y0 < 0) y0 = 0;
        if (y1 >= HEIGHT) y1 = HEIGHT - 1;
        for (int y = y0; y <= y1; y++) {
            setPixel(x, y, on);
        }
    }

    void fillRect(int x, int y, int w, int h, bool on = true) {
        for (int yy = y; yy < y + h; yy++) {
            if ((unsigned)yy >= HEIGHT) continue;
            for (int xx = x; xx < x + w; xx++) {
                setPixel(xx, yy, on);
            }
        }
    }

    void fillRectGray(int x, int y, int w, int h, uint8_t gray) {
        for (int yy = y; yy < y + h; yy++) {
            if ((unsigned)yy >= HEIGHT) continue;
            for (int xx = x; xx < x + w; xx++) {
                setPixelGray(xx, yy, gray);
            }
        }
    }

    void drawRect(int x, int y, int w, int h, bool on = true) {
        drawHLine(x, x + w - 1, y, 1);
        drawHLine(x, x + w - 1, y + h - 1, 1);
        drawVLine(x, y, y + h - 1, on);
        drawVLine(x + w - 1, y, y + h - 1, on);
    }

    void drawLine(int x0, int y0, int x1, int y1, bool on = true) {
        if ((x0 < -64 && x1 < -64) || (x0 > 192 && x1 > 192) ||
            (y0 < -64 && y1 < -64) || (y0 > 128 && y1 > 128)) {
            return;
        }
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            setPixel(x0, y0, on);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    void drawCircle(int xc, int yc, int r, bool on = true) {
        if (r <= 0) return;
        int x = 0, y = r;
        int d = 3 - 2 * r;
        auto plot8 = [&](int cx, int cy, int px, int py) {
            setPixel(cx + px, cy + py, on);
            setPixel(cx - px, cy + py, on);
            setPixel(cx + px, cy - py, on);
            setPixel(cx - px, cy - py, on);
            setPixel(cx + py, cy + px, on);
            setPixel(cx - py, cy + px, on);
            setPixel(cx + py, cy - px, on);
            setPixel(cx - py, cy - px, on);
        };
        while (y >= x) {
            plot8(xc, yc, x, y);
            x++;
            if (d > 0) {
                y--;
                d = d + 4 * (x - y) + 10;
            } else {
                d = d + 4 * x + 6;
            }
        }
    }

    void invertRect(int x, int y, int w, int h) {
        for (int yy = y; yy < y + h; yy++) {
            if ((unsigned)yy >= HEIGHT) continue;
            for (int xx = x; xx < x + w; xx++) {
                xorPixel(xx, yy);
            }
        }
    }

    void fillCircleClipped(int xc, int yc, int r, int maxY, int minY = 0) {
        if (r <= 0) return;
        int r2 = r * r;
        for (int dy = -r; dy <= r; dy++) {
            int py = yc + dy;
            if (py < minY || py > maxY || py >= HEIGHT) continue;
            int dxMax = (int)sqrtf((float)(r2 - dy * dy));
            drawHLine(xc - dxMax, xc + dxMax, py, 1);
        }
    }

    static const uint8_t* glyph(char c) {
        static const uint8_t font[][5] = {
            {0x00,0x00,0x00,0x00,0x00}, // ' ' (0)
            {0x3E,0x51,0x49,0x45,0x3E}, // '0' (1)
            {0x00,0x42,0x7F,0x40,0x00}, // '1'
            {0x42,0x61,0x51,0x49,0x46}, // '2'
            {0x21,0x41,0x45,0x4B,0x31}, // '3'
            {0x18,0x14,0x12,0x7F,0x10}, // '4'
            {0x27,0x45,0x45,0x45,0x39}, // '5'
            {0x3C,0x4A,0x49,0x49,0x30}, // '6'
            {0x01,0x71,0x09,0x05,0x03}, // '7'
            {0x36,0x49,0x49,0x49,0x36}, // '8'
            {0x06,0x49,0x49,0x29,0x1E}, // '9' (10)
            {0x7E,0x11,0x11,0x11,0x7E}, // 'A' (11)
            {0x7F,0x49,0x49,0x49,0x36}, // 'B'
            {0x3E,0x41,0x41,0x41,0x22}, // 'C'
            {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
            {0x7F,0x49,0x49,0x49,0x41}, // 'E'
            {0x7F,0x09,0x09,0x09,0x01}, // 'F'
            {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
            {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
            {0x00,0x41,0x7F,0x41,0x00}, // 'I'
            {0x20,0x40,0x41,0x3F,0x01}, // 'J'
            {0x7F,0x08,0x14,0x22,0x41}, // 'K'
            {0x7F,0x40,0x40,0x40,0x40}, // 'L'
            {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
            {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
            {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
            {0x7F,0x09,0x09,0x09,0x06}, // 'P'
            {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
            {0x7F,0x09,0x19,0x29,0x46}, // 'R'
            {0x46,0x49,0x49,0x49,0x31}, // 'S'
            {0x01,0x01,0x7F,0x01,0x01}, // 'T'
            {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
            {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
            {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
            {0x63,0x14,0x08,0x14,0x63}, // 'X'
            {0x07,0x08,0x70,0x08,0x07}, // 'Y'
            {0x61,0x51,0x49,0x45,0x43}, // 'Z' (36)
            {0x00,0x36,0x36,0x00,0x00}, // ':' (37)
            {0x08,0x08,0x08,0x08,0x08}, // '-' (38)
            {0x14,0x7F,0x14,0x7F,0x14}, // '#' (39)
            {0x00,0x60,0x60,0x00,0x00}, // '.' (40)
            {0x20,0x10,0x08,0x04,0x02}, // '/' (41)
            {0x00,0x41,0x22,0x14,0x08}, // '>' (42)
            {0x08,0x14,0x22,0x41,0x00}, // '<' (43)
            {0x00,0x7F,0x41,0x41,0x00}, // '[' (44)
            {0x00,0x41,0x41,0x7F,0x00}, // ']' (45)
            {0x23,0x13,0x08,0x64,0x62}, // '%' (46)
            {0x08,0x08,0x3E,0x08,0x08}, // '+' (47)
            {0x2A,0x1C,0x3E,0x1C,0x2A}, // '*' (48)
        };
        if (c >= '0' && c <= '9') return font[1 + (c - '0')];
        if (c >= 'A' && c <= 'Z') return font[11 + (c - 'A')];
        if (c >= 'a' && c <= 'z') return font[11 + (c - 'a')];
        if (c == ':') return font[37];
        if (c == '-') return font[38];
        if (c == '#') return font[39];
        if (c == '.') return font[40];
        if (c == '/') return font[41];
        if (c == '>') return font[42];
        if (c == '<') return font[43];
        if (c == '[') return font[44];
        if (c == ']') return font[45];
        if (c == '%') return font[46];
        if (c == '+') return font[47];
        if (c == '*') return font[48];
        return font[0];
    }

    void drawChar(int x, int y, char c, bool on = true) {
        const uint8_t* g = glyph(c);
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) {
                    setPixel(x + col, y + row, on);
                }
            }
        }
    }

    void drawText(int x, int y, const char* str, bool on = true) {
        while (*str) {
            drawChar(x, y, *str++, on);
            x += 6;
        }
    }

    void display() {
        static const uint8_t addrCmds[] = {
            0x00,
            0x21, 0, WIDTH - 1,
            0x22, 0, (HEIGHT / 8) - 1
        };
        i2c_master_write_to_device(I2C_NUM_0, i2cAddr, addrCmds, sizeof(addrCmds), pdMS_TO_TICKS(20));

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i2cAddr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, 0x40, true);
        i2c_master_write(cmd, buffer, BUF_SIZE, true);
        i2c_master_stop(cmd);
        i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
    }
};
