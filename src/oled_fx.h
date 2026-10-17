#pragma once
#include "ssd1306_fast.h"
#include "math3d.h"
#include "wled_net.h"

// ============================================================================
// FX 0: WLED 2D GEQ 16-BAND SPECTRUM ANALYZER (Segmented + Bayer Dither + Hz)
// ============================================================================
inline void renderFxGeqSpectrum(SSD1306Fast& oled, const AudioReactiveState& a, int topY) {
    int barTopY = topY;
    if (!gCfg.hideUi) {
        // Sub-header telemetry bar at y = topY..topY+6
        char info[24];
        snprintf(info, sizeof(info), "%4dHZ PK:%3d", (int)a.dominantHz, a.samplePeak);
        oled.drawText(2, topY, info, true);

        // Beat-reactive stereo VU bar on top-right of stage (x = 84..125)
        int vuW = (int)(a.bassEnv * 40.0f);
        if (vuW > 40) vuW = 40;
        oled.drawRect(84, topY, 42, 7, true);
        oled.fillRect(85, topY + 1, vuW, 5, true);
        barTopY = topY + 9;
    }

    int maxH = 63 - barTopY;
    if (maxH < 10) maxH = 10;

    // 16 Logarithmic FFT Columns across x = 0..127
    for (int b = 0; b < 16; b++) {
        int bx = b * 8 + 1;
        int barH = (a.bins[b] * maxH) / 255;
        int peakH = (a.peakHold[b] * maxH) / 255;

        // Draw segmented LED blocks (2px tall with 1px gap, like WLED 2D GEQ)
        for (int h = 0; h < barH; h += 3) {
            int yy = 63 - h;
            uint8_t shade = (h > (maxH * 2) / 3) ? 255 : ((h > maxH / 3) ? 200 : 155);
            oled.fillRectGray(bx, yy - 1, 6, 2, shade);
        }

        // Floating Peak-Hold Cap
        int py = 63 - peakH;
        if (py < barTopY) py = barTopY;
        oled.drawHLine(bx, bx + 5, py, 1);
    }
}

// ============================================================================
// FX 1: 3D ISOMETRIC WATERFALL SPECTROGRAM
// ============================================================================
inline void renderFxWaterfall3D(SSD1306Fast& oled, const AudioReactiveState& a, int topY) {
    static uint8_t hist[14][16] = {{0}};
    static uint32_t lastPushMs = 0;

    uint32_t now = millis();
    uint32_t stepMs = 55 - (gCfg.sx / 8); // Speed slider controls waterfall scroll rate
    if (now - lastPushMs >= stepMs) {
        lastPushMs = now;
        for (int r = 13; r > 0; r--) {
            memcpy(hist[r], hist[r - 1], 16);
        }
        memcpy(hist[0], a.bins, 16);
    }

    // Render from back-to-front with horizon occlusion buffer
    int minY[128];
    for (int x = 0; x < 128; x++) minY[x] = 64;

    for (int r = 0; r < 14; r++) {
        int baseY = 62 - r * 3;
        int xSkew = r * 2; // Isometric 3D tilt
        int prevX = -1, prevY = -1;

        for (int b = 0; b < 16; b++) {
            int x = 4 + xSkew + b * 6;
            if (x >= 127) x = 127;
            int h = (hist[r][b] * (22 - r / 2)) / 255;
            int y = baseY - h;
            if (y < topY) y = topY;

            if (prevX >= 0) {
                // Draw segment with occlusion against nearer ridges
                int steps = abs(x - prevX);
                if (steps == 0) steps = 1;
                for (int s = 0; s <= steps; s++) {
                    int ix = prevX + ((x - prevX) * s) / steps;
                    int iy = prevY + ((y - prevY) * s) / steps;
                    if ((unsigned)ix < 128 && iy < minY[ix]) {
                        oled.setPixel(ix, iy, true);
                        minY[ix] = iy;
                    }
                }
            }
            prevX = x;
            prevY = y;
        }
    }
}

// ============================================================================
// FX 2: BAYER-DITHERED WLED FIRE2012 & AUDIO PLASMA (Default Effect)
// ============================================================================
inline void renderFxFirePlasma(SSD1306Fast& oled, const AudioReactiveState& a, int topY) {
    static constexpr float TWO_PI = 2.0f * (float)M_PI;
    float t1 = fmodf(a.timeSec * 2.2f, TWO_PI);
    float t2 = fmodf(a.timeSec * 2.2f * 1.6f, TWO_PI);
    float t3 = fmodf(a.totalBeats, TWO_PI);
    float turb = 0.04f + (gCfg.ix * (1.0f / 255.0f)) * 0.06f;
    float bassBoost = a.bassEnv * 110.0f;

    int startY = (topY > 0) ? 14 : 0;
    float invSpanY = 1.0f / (float)(63 - startY);

    for (int y = startY; y < 64; y += 2) {
        float ny = (float)(63 - y) * invSpanY; // 0 at bottom, 1 at top
        for (int x = 0; x < 128; x += 2) {
            int band = (x >> 3) & 15;
            float bandEnergy = a.bins[band] * (1.0f / 255.0f);

            // Combine plasma sine field + rising audio-reactive flame envelope
            float wave = sinf(x * turb + t1) + cosf(y * turb * 1.2f - t2)
                       + sinf((x + y) * 0.04f + t3);
            float plasma = (wave + 3.0f) * (1.0f / 6.0f); // 0..1

            float flameReach = 0.25f + bandEnergy * 0.65f + (bassBoost * (1.0f / 255.0f)) * 0.3f;
            float inten = (flameReach - ny * 0.85f) * 180.0f + plasma * 115.0f;
            int gray = (int)inten;
            if (gray < 0) gray = 0;
            if (gray > 255) gray = 255;

            oled.setPixelGray(x, y, (uint8_t)gray);
            oled.setPixelGray(x + 1, y, (uint8_t)gray);
            oled.setPixelGray(x, y + 1, (uint8_t)gray);
            oled.setPixelGray(x + 1, y + 1, (uint8_t)gray);
        }
    }
}

// ============================================================================
// FX 3: OUTRUN AUDIO (Synthwave Horizon + Audio-Reactive 3D Polyhedron)
// ============================================================================
inline void renderFxOutrunAudio(SSD1306Fast& oled, const AudioReactiveState& a, int topY) {
    static constexpr float TWO_PI = 2.0f * (float)M_PI;
    const int horizonY = 39;
    int sunRadius = 15 + (int)(a.bassEnv * 5.0f);
    oled.fillCircleClipped(64, horizonY, sunRadius, horizonY - 1, topY);
    int stripeOffset = (int)(a.beatPhase * 4.0f);
    for (int y = horizonY - 11 + (stripeOffset & 1); y < horizonY; y += 2) {
        if (y < topY) continue;
        for (int x = 64 - sunRadius - 1; x <= 64 + sunRadius + 1; x++) {
            oled.setPixel(x, y, false);
        }
    }

    oled.drawHLine(0, 127, horizonY, 1);
    float sway = sinf(fmodf(a.totalBeats * (float)M_PI * 0.5f, TWO_PI)) * 14.0f;
    for (int i = -6; i <= 6; i++) {
        oled.drawLine(64 + (int)(sway * 0.3f) + i * 6, horizonY, 64 + (int)sway + i * 24, 63, true);
    }
    float scroll = fmodf(a.totalBeats * (0.6f + a.bassEnv * 0.8f), 1.0f);
    for (int k = 0; k < 6; k++) {
        float normZ = ((float)k + scroll) * (1.0f / 6.0f);
        int y = horizonY + 1 + (int)(normZ * normZ * (63 - horizonY));
        if (y > horizonY && y < 64) oled.drawHLine(0, 127, y, 1);
    }

    static const Vec3 octaVerts[6] = {
        {0,1.25f,0},{0,-1.25f,0},{1.15f,0,0},{-1.15f,0,0},{0,0,1.15f},{0,0,-1.15f}
    };
    static const Edge octaEdges[12] = {
        {0,2},{0,3},{0,4},{0,5},{1,2},{1,3},{1,4},{1,5},{2,4},{4,3},{3,5},{5,2}
    };
    float squashY = 0.85f + a.bassEnv * 0.45f;
    float squashXZ = 0.85f + a.midEnv * 0.35f;
    float rx = fmodf(a.timeSec * 1.9f, TWO_PI);
    float ry = fmodf(a.timeSec * 2.7f, TWO_PI);
    int cy = (topY == 0) ? 34 : 39;
    for (int i = 0; i < 12; i++) {
        Vec3 va = octaVerts[octaEdges[i].a];
        Vec3 vb = octaVerts[octaEdges[i].b];
        va = {va.x * squashXZ, va.y * squashY, va.z * squashXZ};
        vb = {vb.x * squashXZ, vb.y * squashY, vb.z * squashXZ};
        va = rotateXYZ(va, rx, ry, a.highEnv * 0.8f);
        vb = rotateXYZ(vb, rx, ry, a.highEnv * 0.8f);
        drawLine3D(oled, va, vb, 62.0f, 3.3f, 64, cy, topY);
    }
}

// ============================================================================
// FX 4: TUUB LAYT TWIN (Live ESP8266 192.168.1.100 61-LED Strip Mirror + HUD)
// ============================================================================
inline void renderFxTuubTwin(SSD1306Fast& oled, const AudioReactiveState& a, int topY) {
    (void)topY;
    static constexpr float TWO_PI = 2.0f * (float)M_PI;

    // 1. Telemetry Header for ESP8266 "Tuub layt" (y = 15..30)
    if (!gCfg.hideUi) {
        char line1[24], line2[24];
        snprintf(line1, sizeof(line1), "TUUB LAYT %s", gCfg.tuubOnline ? "ONLINE" : "POLL..");
        snprintf(line2, sizeof(line2), "FX:%3d SX:%3d IX:%3d", gCfg.tuubFx, gCfg.tuubSx, gCfg.tuubIx);
        oled.drawText(2, 16, line1, true);
        oled.drawText(2, 25, line2, true);
    }

    // 2. Virtual 61-LED "Tuub layt" Light Strip across x = 2..123, y = 35..46 (2px per LED)
    oled.drawRect(1, 34, 125, 13, true);
    if (gCfg.tuubOn) {
        float tuubSpeed = 0.2f + (gCfg.tuubSx * (1.0f / 255.0f)) * 3.5f;
        float tuubInt   = 0.2f + (gCfg.tuubIx * (1.0f / 255.0f)) * 1.2f;
        float tStrip = fmodf(a.timeSec * tuubSpeed * 3.0f, TWO_PI);
        for (int led = 0; led < 61; led++) {
            int band = (led * 16) / 61;
            float fftVal = a.bins[band] * (1.0f / 255.0f);
            // Lower spatial frequency (0.085f vs 0.22f) makes each horizontal sliding bar ~2.6x longer
            float wave = 0.5f + 0.5f * sinf(led * 0.085f * tuubInt - tStrip);
            int pixBri = (int)((wave * 140.0f + fftVal * 115.0f));
            if (pixBri > 255) pixBri = 255;
            oled.fillRectGray(3 + led * 2, 36, 2, 9, (uint8_t)pixBri);
        }
    } else {
        oled.drawText(34, 37, "STRIP OFF", true);
    }

    // 3. Bottom Mini 16-Band Audio Sync Bars + Brightness Gauge (y = 50..63)
    if (!gCfg.hideUi) {
        oled.drawText(2, 53, "BRI", true);
        oled.drawRect(22, 53, 36, 7, true);
        int bW = (gCfg.tuubBri * 34) / 255;
        if (gCfg.tuubBri > 0 && bW < 2) bW = 2;
        oled.fillRect(23, 54, bW, 5, true);

        for (int i = 0; i < 16; i++) {
            int h = (a.bins[i] * 12) / 255;
            if (h < 1) h = 1;
            oled.fillRect(62 + i * 4, 63 - h, 3, h, true);
        }
    }
}
