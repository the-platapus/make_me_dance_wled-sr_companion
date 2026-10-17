#pragma once
#include "ssd1306_fast.h"
#include "wled_net.h"

enum MenuPageId {
    MENU_MAIN      = 0,
    MENU_EFFECTS   = 1,
    MENU_FX_OPTS   = 2,
    MENU_WLED_CTRL = 3,
    MENU_SETTINGS  = 4
};

struct OledMenuState {
    bool open = false;
    MenuPageId page = MENU_MAIN;
    int cursor = 0;
    uint32_t lastActivityMs = 0;

    void openMenu(uint32_t nowMs) {
        open = true;
        page = MENU_MAIN;
        cursor = 0;
        lastActivityMs = nowMs;
    }

    void closeMenu(uint32_t nowMs) {
        open = false;
        gCfg.hudShowUntilMs = nowMs + 3500;
    }

    int itemCount() const {
        switch (page) {
            case MENU_MAIN:      return 5;
            case MENU_EFFECTS:   return 6;
            case MENU_FX_OPTS:   return 6;
            case MENU_WLED_CTRL: return 7;
            case MENU_SETTINGS:  return 6;
        }
        return 5;
    }

    // 1x Short Press inside menu: advance cursor to next item
    void onShortPress(uint32_t nowMs) {
        lastActivityMs = nowMs;
        int n = itemCount();
        cursor = (cursor + 1) % n;
    }

    static uint8_t stepValue(uint8_t cur, const uint8_t* steps, int count) {
        for (int i = 0; i < count; i++) {
            if (cur < steps[i] - 8) {
                return steps[i];
            }
        }
        return steps[0];
    }

    // Long Press inside menu: select / toggle / step highlighted option
    void onLongPress(uint32_t nowMs) {
        lastActivityMs = nowMs;
        static const uint8_t STEPS_255[]    = { 24, 64, 100, 140, 180, 220, 255 };
        static const uint8_t STEPS_SMOOTH[] = { 0, 40, 90, 140, 185, 220, 250 };
        static const uint8_t STEPS_GAIN[]   = { 70, 100, 130, 160, 200, 250 };
        static const uint8_t STEPS_BRI[]    = { 10, 48, 96, 150, 200, 255 };

        switch (page) {
            case MENU_MAIN:
                if (cursor == 0) { page = MENU_EFFECTS;   cursor = 0; }
                else if (cursor == 1) { page = MENU_FX_OPTS;   cursor = 0; }
                else if (cursor == 2) { page = MENU_WLED_CTRL; cursor = 0; }
                else if (cursor == 3) { page = MENU_SETTINGS;  cursor = 0; }
                else { closeMenu(nowMs); }
                break;

            case MENU_EFFECTS:
                if (cursor == 0) { gCfg.fx = FX_FIRE_PLASMA;  gCfg.autoCycle = false; }
                else if (cursor == 1) { gCfg.fx = FX_GEQ_SPECTRUM; gCfg.autoCycle = false; }
                else if (cursor == 2) { gCfg.fx = FX_WATERFALL_3D; gCfg.autoCycle = false; }
                else if (cursor == 3) { gCfg.fx = FX_OUTRUN_AUDIO; gCfg.autoCycle = false; }
                else if (cursor == 4) { gCfg.fx = FX_TUUB_TWIN;    gCfg.autoCycle = false; }
                else if (cursor == 5) { page = MENU_MAIN; cursor = 0; }
                break;

            case MENU_FX_OPTS:
                if (cursor == 0) {
                    gCfg.sx = stepValue(gCfg.sx, STEPS_255, sizeof(STEPS_255));
                } else if (cursor == 1) {
                    gCfg.ix = stepValue(gCfg.ix, STEPS_255, sizeof(STEPS_255));
                } else if (cursor == 2) {
                    gCfg.smooth = stepValue(gCfg.smooth, STEPS_SMOOTH, sizeof(STEPS_SMOOTH));
                } else if (cursor == 3) {
                    gCfg.gainPct = stepValue(gCfg.gainPct, STEPS_GAIN, sizeof(STEPS_GAIN));
                } else if (cursor == 4) {
                    gCfg.autoCycle = !gCfg.autoCycle;
                } else {
                    page = MENU_MAIN;
                    cursor = 1;
                }
                break;

            case MENU_WLED_CTRL:
                if (cursor == 0) {
                    gCfg.tuubOn = !gCfg.tuubOn;
                    snprintf(gCfg.pendingTuubCmd, sizeof(gCfg.pendingTuubCmd), "&T=2");
                } else if (cursor == 1) {
                    gCfg.tuubBri = stepValue(gCfg.tuubBri, STEPS_BRI, sizeof(STEPS_BRI));
                    snprintf(gCfg.pendingTuubCmd, sizeof(gCfg.pendingTuubCmd), "&A=%d", gCfg.tuubBri);
                } else if (cursor == 2) {
                    gCfg.tuubSx = stepValue(gCfg.tuubSx, STEPS_255, sizeof(STEPS_255));
                    snprintf(gCfg.pendingTuubCmd, sizeof(gCfg.pendingTuubCmd), "&SX=%d", gCfg.tuubSx);
                } else if (cursor == 3) {
                    gCfg.tuubIx = stepValue(gCfg.tuubIx, STEPS_255, sizeof(STEPS_255));
                    snprintf(gCfg.pendingTuubCmd, sizeof(gCfg.pendingTuubCmd), "&IX=%d", gCfg.tuubIx);
                } else if (cursor == 4) {
                    gCfg.syncFromTuub = !gCfg.syncFromTuub;
                } else if (cursor == 5) {
                    gCfg.fx = FX_TUUB_TWIN;
                    gCfg.autoCycle = false;
                    closeMenu(nowMs);
                } else {
                    page = MENU_MAIN;
                    cursor = 2;
                }
                break;

            case MENU_SETTINGS:
                if (cursor == 0) {
                    gCfg.hideUi = !gCfg.hideUi;
                } else if (cursor == 1) {
                    gCfg.invertDisp = !gCfg.invertDisp;
                } else if (cursor == 2) {
                    gCfg.bri = stepValue(gCfg.bri, STEPS_BRI, sizeof(STEPS_BRI));
                } else if (cursor == 3) {
                    gCfg.autoCycle = !gCfg.autoCycle;
                } else if (cursor == 4) {
                    gCfg.ipShowUntilMs = nowMs + 8000;
                    closeMenu(nowMs);
                } else {
                    page = MENU_MAIN;
                    cursor = 3;
                }
                break;
        }
    }

    void getItemText(int idx, char* label, size_t labelSz, char* val, size_t valSz) const {
        label[0] = '\0';
        val[0] = '\0';
        switch (page) {
            case MENU_MAIN:
                if (idx == 0) { snprintf(label, labelSz, "EFFECTS");       snprintf(val, valSz, ">"); }
                if (idx == 1) { snprintf(label, labelSz, "FX OPTIONS");    snprintf(val, valSz, ">"); }
                if (idx == 2) { snprintf(label, labelSz, "WLED TUUB");     snprintf(val, valSz, ">"); }
                if (idx == 3) { snprintf(label, labelSz, "SETTINGS UI");   snprintf(val, valSz, ">"); }
                if (idx == 4) { snprintf(label, labelSz, "EXIT MENU");     snprintf(val, valSz, "[X]"); }
                break;

            case MENU_EFFECTS:
                if (idx == 0) { snprintf(label, labelSz, "PLASMA");        snprintf(val, valSz, gCfg.fx == FX_FIRE_PLASMA  ? "[*]" : ""); }
                if (idx == 1) { snprintf(label, labelSz, "GEQ16 SPEC");    snprintf(val, valSz, gCfg.fx == FX_GEQ_SPECTRUM ? "[*]" : ""); }
                if (idx == 2) { snprintf(label, labelSz, "3D WATERFALL");  snprintf(val, valSz, gCfg.fx == FX_WATERFALL_3D ? "[*]" : ""); }
                if (idx == 3) { snprintf(label, labelSz, "OUTRUN 3D");     snprintf(val, valSz, gCfg.fx == FX_OUTRUN_AUDIO ? "[*]" : ""); }
                if (idx == 4) { snprintf(label, labelSz, "TUUB TWIN");     snprintf(val, valSz, gCfg.fx == FX_TUUB_TWIN    ? "[*]" : ""); }
                if (idx == 5) { snprintf(label, labelSz, "< BACK"); }
                break;

            case MENU_FX_OPTS:
                if (idx == 0) { snprintf(label, labelSz, "SPEED SX");      snprintf(val, valSz, "%3d", gCfg.sx); }
                if (idx == 1) { snprintf(label, labelSz, "INTEN IX");      snprintf(val, valSz, "%3d", gCfg.ix); }
                if (idx == 2) { snprintf(label, labelSz, "SMOOTH");        snprintf(val, valSz, "%3d", gCfg.smooth); }
                if (idx == 3) { snprintf(label, labelSz, "FFT GAIN");      snprintf(val, valSz, "%3d%%", gCfg.gainPct); }
                if (idx == 4) { snprintf(label, labelSz, "AUTO CYCLE");    snprintf(val, valSz, gCfg.autoCycle ? "ON" : "OFF"); }
                if (idx == 5) { snprintf(label, labelSz, "< BACK"); }
                break;

            case MENU_WLED_CTRL:
                if (idx == 0) { snprintf(label, labelSz, "TUUB PWR");      snprintf(val, valSz, gCfg.tuubOn ? "ON" : "OFF"); }
                if (idx == 1) { snprintf(label, labelSz, "TUUB BRI");      snprintf(val, valSz, "%3d", gCfg.tuubBri); }
                if (idx == 2) { snprintf(label, labelSz, "TUUB SPD");      snprintf(val, valSz, "%3d", gCfg.tuubSx); }
                if (idx == 3) { snprintf(label, labelSz, "TUUB INT");      snprintf(val, valSz, "%3d", gCfg.tuubIx); }
                if (idx == 4) { snprintf(label, labelSz, "MIRROR TUUB");   snprintf(val, valSz, gCfg.syncFromTuub ? "ON" : "OFF"); }
                if (idx == 5) { snprintf(label, labelSz, "OPEN TWIN");     snprintf(val, valSz, ">"); }
                if (idx == 6) { snprintf(label, labelSz, "< BACK"); }
                break;

            case MENU_SETTINGS:
                if (idx == 0) { snprintf(label, labelSz, "HIDE UI");       snprintf(val, valSz, gCfg.hideUi ? "ON" : "OFF"); }
                if (idx == 1) { snprintf(label, labelSz, "INVERT OLED");   snprintf(val, valSz, gCfg.invertDisp ? "ON" : "OFF"); }
                if (idx == 2) { snprintf(label, labelSz, "CONTRAST");      snprintf(val, valSz, "%3d", gCfg.bri); }
                if (idx == 3) { snprintf(label, labelSz, "AUTO CYCLE");    snprintf(val, valSz, gCfg.autoCycle ? "ON" : "OFF"); }
                if (idx == 4) { snprintf(label, labelSz, "IP:%s", gCfg.ipStr); }
                if (idx == 5) { snprintf(label, labelSz, "< BACK"); }
                break;
        }
    }

    void render(SSD1306Fast& oled, uint32_t nowMs) {
        if (nowMs - lastActivityMs > 15000) {
            closeMenu(nowMs);
            return;
        }

        // Top Menu Title Bar (y = 0..11)
        const char* title = "O-LED MENU";
        if (page == MENU_EFFECTS)   title = "SELECT EFFECT";
        if (page == MENU_FX_OPTS)   title = "FX OPTIONS";
        if (page == MENU_WLED_CTRL) title = "WLED TUUB CTRL";
        if (page == MENU_SETTINGS)  title = "SETTINGS UI";

        oled.fillRect(0, 0, 128, 11, true);
        oled.drawText(3, 2, title, false);

        // Right-aligned page indicator (e.g. "1/5")
        int total = itemCount();
        char idxBuf[24];
        snprintf(idxBuf, sizeof(idxBuf), "%d/%d", cursor + 1, total);
        int rx = 126 - (int)strlen(idxBuf) * 6;
        oled.drawText(rx, 2, idxBuf, false);

        // 4 visible scrolling menu rows across y = 13..60 (12px per row)
        int firstVis = cursor - 1;
        if (firstVis > total - 4) firstVis = total - 4;
        if (firstVis < 0) firstVis = 0;

        for (int row = 0; row < 4; row++) {
            int itemIdx = firstVis + row;
            if (itemIdx >= total) break;

            int ry = 14 + row * 12;
            char label[24], val[16];
            getItemText(itemIdx, label, sizeof(label), val, sizeof(val));

            oled.drawText(5, ry + 2, label, true);
            if (val[0] != '\0') {
                int vx = 123 - (int)strlen(val) * 6;
                oled.drawText(vx, ry + 2, val, true);
            }

            if (itemIdx == cursor) {
                // Highlight selected row using XOR inversion for crisp white bar + black text
                oled.invertRect(1, ry, 126, 11);
            }
        }
    }
};
