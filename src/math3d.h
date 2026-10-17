#pragma once
#include "ssd1306_fast.h"

struct Vec3 {
    float x, y, z;
};

struct Vec2i {
    int x, y;
    bool visible;
};

struct Edge {
    uint8_t a, b;
};

inline Vec3 rotateX(const Vec3& v, float a) {
    float c = cosf(a), s = sinf(a);
    return {v.x, v.y * c - v.z * s, v.y * s + v.z * c};
}

inline Vec3 rotateY(const Vec3& v, float a) {
    float c = cosf(a), s = sinf(a);
    return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

inline Vec3 rotateZ(const Vec3& v, float a) {
    float c = cosf(a), s = sinf(a);
    return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}

inline Vec3 rotateXYZ(Vec3 v, float rx, float ry, float rz) {
    v = rotateX(v, rx);
    v = rotateY(v, ry);
    v = rotateZ(v, rz);
    return v;
}

inline Vec2i project3D(const Vec3& v, float fov = 64.0f, float camDist = 3.2f, int cx = 64, int cy = 40) {
    float z = v.z + camDist;
    if (z < 0.25f) {
        return {0, 0, false};
    }
    float invZ = fov / z;
    int sx = cx + (int)lroundf(v.x * invZ);
    int sy = cy - (int)lroundf(v.y * invZ);
    return {sx, sy, true};
}

inline void drawLine3D(SSD1306Fast& oled, const Vec3& a, const Vec3& b, float fov = 64.0f, float camDist = 3.2f, int cx = 64, int cy = 40, int minY = 0) {
    Vec2i pa = project3D(a, fov, camDist, cx, cy);
    Vec2i pb = project3D(b, fov, camDist, cx, cy);
    if (!pa.visible || !pb.visible) return;
    if (pa.y < minY && pb.y < minY) return;
    int y0 = pa.y < minY ? minY : pa.y;
    int y1 = pb.y < minY ? minY : pb.y;
    oled.drawLine(pa.x, y0, pb.x, y1, true);
}

// Unified Audio + 128 BPM Synth State
struct AudioReactiveState {
    float timeSec = 0.0f;
    float totalBeats = 0.0f;
    float beatPhase = 0.0f;
    uint32_t beatIndex = 0;
    uint32_t barIndex = 0;

    // 16 FFT bands (0..255) from WLED UDP 11988 with adjustable smoothness
    uint8_t bins[16] = {0};
    uint8_t targetBins[16] = {0};
    float smoothBins[16] = {0.0f};
    uint8_t peakHold[16] = {0};
    float bassEnv = 0.0f;     // 0.0 .. 1.0 (bins 0..2)
    float midEnv = 0.0f;      // 0.0 .. 1.0 (bins 4..9)
    float highEnv = 0.0f;     // 0.0 .. 1.0 (bins 10..15)
    float targetBass = 0.0f;
    float targetMid = 0.0f;
    float targetHigh = 0.0f;
    float kickEnv = 0.0f;     // 0.0 .. 1.0 beat/peak impulse
    float snareEnv = 0.0f;    // 0.0 .. 1.0
    uint8_t samplePeak = 0;   // 0..255
    float dominantHz = 0.0f;
    float targetHz = 0.0f;
    bool liveAudio = false;
    uint32_t lastUdpMs = 0;
    uint32_t udpPackets = 0;
};
