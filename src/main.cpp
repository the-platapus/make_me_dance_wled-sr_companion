#include "ssd1306_fast.h"
#include "math3d.h"
#include "wled_net.h"
#include "oled_fx.h"
#include "oled_menu.h"

SSD1306Fast oled;
OledWledConfig gCfg;
AudioReactiveState gAudio;
OledMenuState gMenu;
int gCurrentFps = 60;

static constexpr int PIN_SDA = 22;
static constexpr int PIN_SCL = 23;
static constexpr int PIN_BOOT_BTN = 0;

// Top HUD Bar (y = 0..13): Appears only when non-FPS info changes (and when Hide UI is OFF)
static void renderHud(uint32_t nowMs) {
    oled.fillRect(0, 0, 128, 14, false);

    if (nowMs < gCfg.hudBannerUntilMs && gCfg.hudBannerText[0] != '\0') {
        oled.drawText(2, 2, gCfg.hudBannerText, true);
    } else if (nowMs < gCfg.ipShowUntilMs) {
        oled.drawText(2, 2, "IP:", true);
        oled.drawText(20, 2, gCfg.ipStr, true);
    } else {
        oled.drawText(2, 2, FX_NAMES[gCfg.fx % FX_COUNT], true);
        oled.drawText(48, 2, gAudio.liveAudio ? "SR" : "--", true);

        // 4 mini equalizer bars in HUD (x = 64..84) — flat 1px dot when silent
        for (int i = 0; i < 4; i++) {
            int h = 1 + (gAudio.bins[i * 4] * 9) / 255;
            oled.fillRect(64 + i * 5, 11 - h, 3, h, true);
        }

        // Beat circle at x = 92 (expands only on real audio kick)
        int r = (gAudio.kickEnv > 0.45f) ? 4 : 2;
        oled.drawCircle(92, 5, r, true);
    }

    char fpsBuf[16];
    int clampedFps = gCurrentFps < 0 ? 0 : (gCurrentFps > 99 ? 99 : gCurrentFps);
    snprintf(fpsBuf, sizeof(fpsBuf), "%2dF", clampedFps);
    oled.drawText(104, 2, fpsBuf, true);

    int barHalf = (int)(gAudio.kickEnv * 63.0f);
    if (barHalf > 63) barHalf = 63;
    oled.drawHLine(0, 127, 13, 2);
    oled.drawHLine(64 - barHalf, 64 + barHalf, 12, 1);
}

extern "C" void app_main(void) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_BOOT_BTN);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    bool ok = oled.begin(PIN_SDA, PIN_SCL, 400000);
    printf("\n==============================================\n");
    printf(" O-LED: SOUND-REACTIVE WLED FOR 128x64 OLED   \n");
    printf("==============================================\n");
    printf("SSD1306 I2C (SDA=%d, SCL=%d): %s at 0x%02X\n",
           oled.activeSda, oled.activeScl, ok ? "CONNECTED" : "NOT ACKED", oled.i2cAddr);

    initWledNetwork();

    uint32_t lastFpsTime = millis();
    uint32_t lastLogTime = 0;
    uint32_t lastSwitchBeat = 0;
    int frameCount = 0;

    // Button state machine (Short press = next FX / next menu item, Long press >=520ms = open menu / select item)
    int lastBtnState = 1;
    uint32_t btnDownMs = 0;
    bool btnLongHandled = false;

    // Track displayed HUD state so top bar ONLY appears when non-FPS info changes
    uint8_t prevHudFx = gCfg.fx;
    bool prevHudLive = gAudio.liveAudio;
    bool prevHudAuto = gCfg.autoCycle;
    char prevHudIp[20] = {0};
    strncpy(prevHudIp, gCfg.ipStr, sizeof(prevHudIp) - 1);

    // Track NVS-persisted settings for debounced flash save (1.5s after last user edit)
    uint8_t nvsFx = gCfg.fx, nvsSx = gCfg.sx, nvsIx = gCfg.ix, nvsBri = gCfg.bri, nvsGain = gCfg.gainPct, nvsSmooth = gCfg.smooth;
    bool nvsAuto = gCfg.autoCycle, nvsHide = gCfg.hideUi, nvsInv = gCfg.invertDisp, nvsSync = gCfg.syncFromTuub;
    bool nvsDirty = false;
    uint32_t nvsChangeMs = 0;

    while (true) {
        uint32_t now = millis();
        updateAudioReactiveState(now);

        // BOOT button (GPIO 0, active-low)
        int btnState = gpio_get_level((gpio_num_t)PIN_BOOT_BTN);
        if (lastBtnState == 1 && btnState == 0) {
            // Button pressed down
            btnDownMs = now;
            btnLongHandled = false;
        } else if (btnState == 0 && !btnLongHandled && (now - btnDownMs >= 520)) {
            // Long press held >= 520 ms
            btnLongHandled = true;
            if (gMenu.open) {
                gMenu.onLongPress(now);
                printf("[MENU] Long-press select (page=%d, cursor=%d)\n", gMenu.page, gMenu.cursor);
            } else {
                gMenu.openMenu(now);
                printf("[BTN] Long-press -> Opened O-LED Menu UI\n");
            }
        } else if (lastBtnState == 0 && btnState == 1) {
            // Button released
            if (!btnLongHandled && (now - btnDownMs >= 25)) {
                if (gMenu.open) {
                    gMenu.onShortPress(now);
                } else {
                    gCfg.fx = (gCfg.fx + 1) % FX_COUNT;
                    lastSwitchBeat = gAudio.beatIndex;
                    printf("[BTN] Short-press -> FX %d (%s)\n", gCfg.fx, FX_NAMES[gCfg.fx]);
                }
            }
        }
        lastBtnState = btnState;

        // Auto-cycle effects every 24 beats if autoCycle is enabled and menu is closed
        if (!gMenu.open && gCfg.autoCycle && (gAudio.beatIndex - lastSwitchBeat >= 24)) {
            gCfg.fx = (gCfg.fx + 1) % FX_COUNT;
            nvsFx = gCfg.fx; // Don't wear flash on automatic beat switches
            lastSwitchBeat = gAudio.beatIndex;
        }

        // Detect user setting changes for debounced NVS flash persistence
        if (gCfg.fx != nvsFx || gCfg.sx != nvsSx || gCfg.ix != nvsIx ||
            gCfg.bri != nvsBri || gCfg.gainPct != nvsGain || gCfg.smooth != nvsSmooth ||
            gCfg.autoCycle != nvsAuto || gCfg.hideUi != nvsHide ||
            gCfg.invertDisp != nvsInv || gCfg.syncFromTuub != nvsSync) {
            nvsFx = gCfg.fx; nvsSx = gCfg.sx; nvsIx = gCfg.ix;
            nvsBri = gCfg.bri; nvsGain = gCfg.gainPct; nvsSmooth = gCfg.smooth;
            nvsAuto = gCfg.autoCycle; nvsHide = gCfg.hideUi;
            nvsInv = gCfg.invertDisp; nvsSync = gCfg.syncFromTuub;
            nvsDirty = true;
            nvsChangeMs = now;
        }
        if (nvsDirty && (now - nvsChangeMs >= 1500)) {
            nvsDirty = false;
            saveConfigToNvs();
        }

        // Detect changes in any displayed top-bar info EXCEPT FPS
        if (gCfg.fx != prevHudFx ||
            gAudio.liveAudio != prevHudLive ||
            gCfg.autoCycle != prevHudAuto ||
            strcmp(gCfg.ipStr, prevHudIp) != 0) {
            prevHudFx = gCfg.fx;
            prevHudLive = gAudio.liveAudio;
            prevHudAuto = gCfg.autoCycle;
            strncpy(prevHudIp, gCfg.ipStr, sizeof(prevHudIp) - 1);
            gCfg.hudShowUntilMs = now + 3500;
        }

        oled.setPower(gCfg.powerOn);
        oled.setContrast(gCfg.bri);
        oled.setInvert(gCfg.invertDisp);

        if (gCfg.powerOn) {
            oled.clear();
            if (gMenu.open) {
                gMenu.render(oled, now);
            } else {
                bool showHud = !gCfg.hideUi &&
                               (now < gCfg.hudShowUntilMs ||
                                now < gCfg.ipShowUntilMs ||
                                now < gCfg.hudBannerUntilMs);
                int topY = showHud ? 15 : 0;

                switch (gCfg.fx % FX_COUNT) {
                    case FX_GEQ_SPECTRUM: renderFxGeqSpectrum(oled, gAudio, topY); break;
                    case FX_WATERFALL_3D: renderFxWaterfall3D(oled, gAudio, topY); break;
                    case FX_FIRE_PLASMA:  renderFxFirePlasma(oled, gAudio, topY);  break;
                    case FX_OUTRUN_AUDIO: renderFxOutrunAudio(oled, gAudio, topY); break;
                    case FX_TUUB_TWIN:    renderFxTuubTwin(oled, gAudio, topY);    break;
                }
                if (showHud) {
                    renderHud(now);
                }
            }
            oled.display();
        }

        frameCount++;
        if (now - lastFpsTime >= 1000) {
            gCurrentFps = frameCount;
            frameCount = 0;
            lastFpsTime = now;
        }

        if (now - lastLogTime >= 3000) {
            lastLogTime = now;
            printf("[O-LED] IP=%s | FX=%s | Mode=%s | Pkts=%lu | Tuub=%s(fx=%d,bri=%d) | FPS=%d\n",
                   gCfg.ipStr, FX_NAMES[gCfg.fx], gAudio.liveAudio ? "LIVE-UDP" : "IDLE",
                   (unsigned long)gAudio.udpPackets,
                   gCfg.tuubOnline ? "OK" : "WAIT", gCfg.tuubFx, gCfg.tuubBri, gCurrentFps);
        }

        vTaskDelay(1);
    }
}
