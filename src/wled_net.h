#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "math3d.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID_SECRET
#define WIFI_SSID_SECRET "YOUR_WIFI_SSID"
#endif
#ifndef WIFI_PASS_SECRET
#define WIFI_PASS_SECRET "YOUR_WIFI_PASSWORD"
#endif
#ifndef TUUB_LAYT_IP_SECRET
#define TUUB_LAYT_IP_SECRET "192.168.1.100"
#endif

static constexpr const char* WIFI_SSID_DEFAULT = WIFI_SSID_SECRET;
static constexpr const char* WIFI_PASS_DEFAULT = WIFI_PASS_SECRET;
static constexpr const char* TUUB_LAYT_IP      = TUUB_LAYT_IP_SECRET;
static constexpr uint16_t    WLED_AUDIO_PORT   = 11988;

enum OledFxId {
    FX_GEQ_SPECTRUM   = 0,
    FX_WATERFALL_3D   = 1,
    FX_FIRE_PLASMA    = 2,
    FX_OUTRUN_AUDIO   = 3,
    FX_TUUB_TWIN      = 4,
    FX_COUNT          = 5
};

static const char* FX_NAMES[FX_COUNT] = {
    "GEQ16",
    "WATER3D",
    "PLASMA",
    "OUTRUN",
    "TUUBTWN"
};

struct OledWledConfig {
    bool powerOn = true;
    uint8_t bri = 255;       // OLED contrast (0..255)
    uint8_t fx = FX_TUUB_TWIN; // Default screen on boot: TUUBTWN slider screen
    uint8_t sx = 140;        // Effect speed (0..255)
    uint8_t ix = 160;        // Effect intensity (0..255)
    uint8_t gainPct = 130;   // FFT Audio Gain (50..300%)
    uint8_t smooth = 140;    // WLED-SR Audio Input Smoothness (0..255)
    bool autoCycle = false;  // Off by default so default screen stays active
    bool invertDisp = false;
    bool hideUi = false;     // Hide all on-screen HUD & text overlays
    bool syncFromTuub = false;

    // Network & HUD status
    bool wifiConnected = false;
    char ipStr[20] = "CONNECTING...";
    uint32_t ipShowUntilMs = 0;
    uint32_t hudShowUntilMs = 4000;
    char hudBannerText[24] = {0};
    uint32_t hudBannerUntilMs = 0;

    // ESP8266 "Tuub layt" (192.168.1.100) Twin state
    bool tuubOnline = false;
    bool tuubOn = true;
    uint8_t tuubBri = 3;
    int tuubFx = 207;
    uint8_t tuubSx = 5;
    uint8_t tuubIx = 126;
    uint8_t tuubCol[3] = {255, 160, 0};
    char tuubName[24] = "TUUB LAYT";
    char pendingTuubCmd[96] = {0};
};

extern OledWledConfig gCfg;
extern AudioReactiveState gAudio;
extern int gCurrentFps;

// ============================================================================
// NVS Flash Persistence for O-LED Settings
// ============================================================================
struct SavedOledConfig {
    uint32_t magic;
    uint8_t fx;
    uint8_t sx;
    uint8_t ix;
    uint8_t bri;
    uint8_t gainPct;
    uint8_t smooth;
    uint8_t flags; // bit0: autoCycle, bit1: hideUi, bit2: invertDisp, bit3: syncFromTuub
};

static constexpr uint32_t OLED_CFG_MAGIC = 0x4F4C4547; // "OLEG" v3

inline void saveConfigToNvs() {
    nvs_handle_t h;
    if (nvs_open("oled_cfg", NVS_READWRITE, &h) == ESP_OK) {
        SavedOledConfig s = {};
        s.magic   = OLED_CFG_MAGIC;
        s.fx      = gCfg.fx % FX_COUNT;
        s.sx      = gCfg.sx;
        s.ix      = gCfg.ix;
        s.bri     = gCfg.bri;
        s.gainPct = gCfg.gainPct;
        s.smooth  = gCfg.smooth;
        s.flags   = (gCfg.autoCycle    ? 0x01 : 0) |
                    (gCfg.hideUi       ? 0x02 : 0) |
                    (gCfg.invertDisp   ? 0x04 : 0) |
                    (gCfg.syncFromTuub ? 0x08 : 0);
        nvs_set_blob(h, "cfg", &s, sizeof(s));
        nvs_commit(h);
        nvs_close(h);
        printf("[NVS] Saved settings (fx=%d, sx=%d, ix=%d, bri=%d, gain=%d, smooth=%d, flags=0x%02X)\n",
               s.fx, s.sx, s.ix, s.bri, s.gainPct, s.smooth, s.flags);
    }
}

inline void loadConfigFromNvs() {
    nvs_handle_t h;
    if (nvs_open("oled_cfg", NVS_READONLY, &h) == ESP_OK) {
        SavedOledConfig s = {};
        size_t sz = sizeof(s);
        if (nvs_get_blob(h, "cfg", &s, &sz) == ESP_OK && sz == sizeof(s) && s.magic == OLED_CFG_MAGIC) {
            gCfg.fx           = s.fx % FX_COUNT;
            gCfg.sx           = s.sx;
            gCfg.ix           = s.ix;
            gCfg.bri          = s.bri > 0 ? s.bri : 255;
            gCfg.gainPct      = s.gainPct >= 50 ? s.gainPct : 130;
            gCfg.smooth       = s.smooth;
            gCfg.autoCycle    = (s.flags & 0x01) != 0;
            gCfg.hideUi       = (s.flags & 0x02) != 0;
            gCfg.invertDisp   = (s.flags & 0x04) != 0;
            gCfg.syncFromTuub = (s.flags & 0x08) != 0;
            printf("[NVS] Loaded saved settings (fx=%d [%s], sx=%d, ix=%d, smooth=%d, hideUi=%d)\n",
                   gCfg.fx, FX_NAMES[gCfg.fx], gCfg.sx, gCfg.ix, gCfg.smooth, gCfg.hideUi);
        }
        nvs_close(h);
    }
}

// Parse integer between <tag>...</tag> from WLED /win XML
static inline int parseXmlTag(const char* xml, const char* tag, int defVal) {
    char openTag[16], closeTag[16];
    snprintf(openTag, sizeof(openTag), "<%s>", tag);
    snprintf(closeTag, sizeof(closeTag), "</%s>", tag);
    const char* p = strstr(xml, openTag);
    if (!p) return defVal;
    p += strlen(openTag);
    return atoi(p);
}

// ============================================================================
// UDP 11988 WLED-SR v2 ("00002") Audio-Reactive Receiver Task
// ============================================================================
#pragma pack(push, 1)
struct WledAudioPacketV2 {
    char header[6];          // "00002\0"
    uint8_t gap1[2];
    float sampleRaw;
    float sampleSmoothed;
    uint8_t samplePeak;
    uint8_t reserved;
    uint8_t fftResult[16];
    uint8_t gap2[2];
    float fftMagnitude;
    float fftMajorPeak;
};
#pragma pack(pop)

static void udpAudioTask(void* arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        printf("[UDP-11988] Failed to create socket\n");
        vTaskDelete(NULL);
        return;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(WLED_AUDIO_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[UDP-11988] Bind failed\n");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    // Join WLED Audio Sync multicast group 239.0.0.1
    struct ip_mreq mreq = {};
    mreq.imr_multiaddr.s_addr = inet_addr("239.0.0.1");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    printf("[UDP-11988] Listening for WLED-SR v2 packets on port %d (+ multicast 239.0.0.1)\n", WLED_AUDIO_PORT);

    uint8_t rxBuf[128];
    while (true) {
        struct sockaddr_in srcAddr;
        socklen_t socklen = sizeof(srcAddr);
        int len = recvfrom(sock, rxBuf, sizeof(rxBuf), 0, (struct sockaddr*)&srcAddr, &socklen);
        if (len >= 40 && memcmp(rxBuf, "00002", 5) == 0) {
            const WledAudioPacketV2* pkt = (const WledAudioPacketV2*)rxBuf;
            float gain = gCfg.gainPct * 0.01f;

            float bassSum = 0.0f, midSum = 0.0f, highSum = 0.0f;
            int totalEnergy = 0;
            for (int i = 0; i < 16; i++) {
                int v = (int)(pkt->fftResult[i] * gain);
                if (v > 255) v = 255;
                gAudio.targetBins[i] = (uint8_t)v;
                totalEnergy += v;
                if (v > gAudio.peakHold[i]) {
                    gAudio.peakHold[i] = (uint8_t)v;
                }
                if (i < 3) bassSum += v;
                else if (i < 10) midSum += v;
                else highSum += v;
            }
            gAudio.targetBass = (bassSum / (3.0f * 255.0f));
            gAudio.targetMid  = (midSum / (7.0f * 255.0f));
            gAudio.targetHigh = (highSum / (6.0f * 255.0f));
            gAudio.samplePeak = pkt->samplePeak;
            if (pkt->samplePeak > 0 || gAudio.targetBass > 0.45f) {
                float cand = gAudio.targetBass > (pkt->samplePeak / 255.0f) ? gAudio.targetBass : (pkt->samplePeak / 255.0f);
                if (cand > gAudio.kickEnv) gAudio.kickEnv = cand;
            }
            if (gAudio.targetMid > gAudio.snareEnv) {
                gAudio.snareEnv = gAudio.targetMid;
            }
            if (totalEnergy > 8 || pkt->samplePeak > 0) {
                if (pkt->fftMajorPeak >= 15.0f && pkt->fftMajorPeak <= 20000.0f) {
                    gAudio.targetHz = pkt->fftMajorPeak;
                }
                gAudio.lastUdpMs = millis();
                gAudio.liveAudio = true;
            }
            gAudio.udpPackets++;
        }
    }
}

// ============================================================================
// ESP8266 "Tuub layt" (192.168.1.100) Twin Polling & Control Task
// ============================================================================
static void tuubTwinTask(void* arg) {
    char rxBuf[512];
    while (true) {
        if (gCfg.wifiConnected) {
            char reqStr[192];
            if (gCfg.pendingTuubCmd[0] != '\0') {
                snprintf(reqStr, sizeof(reqStr),
                         "GET /win%s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                         gCfg.pendingTuubCmd, TUUB_LAYT_IP);
                gCfg.pendingTuubCmd[0] = '\0';
            } else {
                snprintf(reqStr, sizeof(reqStr),
                         "GET /win HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                         TUUB_LAYT_IP);
            }

            int s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            if (s >= 0) {
                struct timeval tv = { .tv_sec = 1, .tv_usec = 200000 };
                setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

                struct sockaddr_in dest = {};
                dest.sin_family = AF_INET;
                dest.sin_port = htons(80);
                dest.sin_addr.s_addr = inet_addr(TUUB_LAYT_IP);

                if (connect(s, (struct sockaddr*)&dest, sizeof(dest)) == 0) {
                    send(s, reqStr, strlen(reqStr), 0);
                    int total = 0;
                    while (total < (int)sizeof(rxBuf) - 1) {
                        int r = recv(s, rxBuf + total, sizeof(rxBuf) - 1 - total, 0);
                        if (r <= 0) break;
                        total += r;
                    }
                    rxBuf[total] = '\0';
                    const char* xml = strstr(rxBuf, "<vs>");
                    if (xml) {
                        int ac = parseXmlTag(xml, "ac", gCfg.tuubBri);
                        gCfg.tuubOn = (ac > 0);
                        if (ac > 0) gCfg.tuubBri = (uint8_t)ac;
                        gCfg.tuubFx = parseXmlTag(xml, "fx", gCfg.tuubFx);
                        gCfg.tuubSx = (uint8_t)parseXmlTag(xml, "sx", gCfg.tuubSx);
                        gCfg.tuubIx = (uint8_t)parseXmlTag(xml, "ix", gCfg.tuubIx);
                        gCfg.tuubOnline = true;

                        if (gCfg.syncFromTuub) {
                            gCfg.sx = gCfg.tuubSx;
                            gCfg.ix = gCfg.tuubIx;
                        }
                    }
                } else {
                    gCfg.tuubOnline = false;
                }
                close(s);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

// ============================================================================
// Embedded WLED-Style Dark-Mode Web UI & JSON/XML API Server
// ============================================================================
static const char INDEX_HTML[] = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>O-LED // WLED-SR for SSD1306</title>
<style>
:root{--bg:#0d1117;--card:#161b22;--acc:#00e5ff;--acc2:#ff2a85;--txt:#e6edf3;--mut:#8b949e}
*{box-sizing:border-box;font-family:system-ui,-apple-system,sans-serif}
body{margin:0;background:var(--bg);color:var(--txt);padding:14px;max-width:760px;margin-inline:auto}
header{display:flex;justify-content:space-between;align-items:center;background:var(--card);padding:12px 16px;border-radius:12px;border:1px solid #30363d;margin-bottom:14px}
.logo{font-weight:800;font-size:1.25rem;letter-spacing:1px;color:var(--acc)}
.badge{padding:4px 10px;border-radius:99px;font-size:.75rem;font-weight:700;background:#21262d}
.badge.live{background:#00e5ff22;color:var(--acc);border:1px solid var(--acc)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px;margin-bottom:14px}
.card{background:var(--card);border:1px solid #30363d;border-radius:12px;padding:14px}
h3{margin:0 0 10px;font-size:.9rem;text-transform:uppercase;letter-spacing:.8px;color:var(--mut)}
.fx-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(135px,1fr));gap:8px}
.fx-btn{background:#21262d;border:1px solid #30363d;color:var(--txt);padding:10px;border-radius:8px;cursor:pointer;font-weight:600;text-align:left;transition:.15s}
.fx-btn.active{background:#00e5ff22;border-color:var(--acc);color:var(--acc)}
.slider-row{margin-bottom:10px}
.slider-row label{display:flex;justify-content:space-between;font-size:.82rem;margin-bottom:4px}
input[type=range]{width:100%;accent-color:var(--acc)}
.toggles{display:flex;flex-wrap:wrap;gap:8px;margin-top:8px}
.t-btn{flex:1;min-width:110px;padding:8px 10px;border-radius:8px;border:1px solid #30363d;background:#21262d;color:var(--txt);cursor:pointer;font-weight:600;font-size:.8rem}
.t-btn.on{background:#ff2a8528;border-color:var(--acc2);color:#ff7ab8}
.fft-bars{display:flex;align-items:flex-end;gap:4px;height:68px;padding-top:8px}
.bar{flex:1;background:linear-gradient(to top,var(--acc),var(--acc2));border-radius:2px 2px 0 0;min-height:1px;transition:height .08s}
</style>
</head>
<body>
<header>
  <div>
    <div class="logo">O-LED <span style="font-size:.8rem;color:var(--mut)">128x64 SR</span></div>
    <div id="sub" style="font-size:.75rem;color:var(--mut)">Connecting...</div>
  </div>
  <div id="modeBadge" class="badge">WAITING FOR AUDIO</div>
</header>

<div class="card" style="margin-bottom:14px">
  <h3>Live 16-Band WLED-SR Spectrum (<span id="hz">0</span> Hz)</h3>
  <div class="fft-bars" id="fft"></div>
</div>

<div class="grid">
  <div class="card">
    <h3>OLED Effect Controls</h3>
    <div class="slider-row">
      <label><span>Effect Speed (SX)</span><span id="sxV">140</span></label>
      <input type="range" min="0" max="255" id="sx" oninput="send('sx',this.value)">
    </div>
    <div class="slider-row">
      <label><span>Effect Intensity (IX)</span><span id="ixV">160</span></label>
      <input type="range" min="0" max="255" id="ix" oninput="send('ix',this.value)">
    </div>
    <div class="slider-row">
      <label><span>Audio FFT Gain (%)</span><span id="gnV">130%</span></label>
      <input type="range" min="50" max="300" id="gain" oninput="send('gain',this.value)">
    </div>
    <div class="slider-row">
      <label><span>Audio Smoothness</span><span id="smV">140</span></label>
      <input type="range" min="0" max="255" id="smooth" oninput="send('smooth',this.value)">
    </div>
    <div class="slider-row">
      <label><span>OLED Contrast (BRI)</span><span id="brV">255</span></label>
      <input type="range" min="1" max="255" id="bri" oninput="send('bri',this.value)">
    </div>
    <div class="toggles">
      <button class="t-btn" id="btnPwr" onclick="tog('on')">OLED Power</button>
      <button class="t-btn" id="btnAuto" onclick="tog('auto')">Auto-Cycle</button>
      <button class="t-btn" id="btnHide" onclick="tog('hide_ui')">Hide UI</button>
      <button class="t-btn" id="btnInv" onclick="tog('inv')">Invert</button>
      <button class="t-btn" id="btnSync" onclick="tog('sync_tuub')">Mirror Tuub SX/IX</button>
    </div>
  </div>

  <div class="card">
    <h3>ESP8266 Twin: Tuub layt (192.168.1.100)</h3>
    <div id="tuubStat" style="font-size:.82rem;color:var(--mut);margin-bottom:10px">Polling 192.168.1.100...</div>
    <div class="slider-row">
      <label><span>Tuub layt Brightness</span><span id="tBrV">3</span></label>
      <input type="range" min="0" max="255" id="tBri" onchange="send('tuub_bri',this.value)">
    </div>
    <div class="slider-row">
      <label><span>Tuub layt Speed (SX)</span><span id="tSxV">5</span></label>
      <input type="range" min="0" max="255" id="tSx" onchange="send('tuub_sx',this.value)">
    </div>
    <div class="slider-row">
      <label><span>Tuub layt Intensity (IX)</span><span id="tIxV">126</span></label>
      <input type="range" min="0" max="255" id="tIx" onchange="send('tuub_ix',this.value)">
    </div>
    <div class="toggles">
      <button class="t-btn" onclick="send('fx',4)">Show Twin on OLED</button>
      <button class="t-btn" onclick="send('tuub_on',2)">Toggle Tuub Power</button>
    </div>
  </div>
</div>

<div class="card">
  <h3>O-LED Sound-Reactive Effects</h3>
  <div class="fx-grid" id="fxGrid"></div>
</div>

<script>
const FX=["0: GEQ Spectrum","1: 3D Waterfall","2: Bayer Fire/Plasma","3: Outrun Audio","4: Tuub layt Twin"];
let st={};
const fftEl=document.getElementById('fft');
for(let i=0;i<16;i++){const b=document.createElement('div');b.className='bar';fftEl.appendChild(b);}
const fxEl=document.getElementById('fxGrid');
FX.forEach((n,i)=>{
  const b=document.createElement('button');
  b.className='fx-btn';b.id='fx'+i;b.textContent=n;
  b.onclick=()=>send('fx',i);
  fxEl.appendChild(b);
});
function send(k,v){fetch(`/api/state?${k}=${v}`).then(r=>r.json()).then(render);}
function tog(k){send(k,st[k]?0:1);}
function render(d){
  st=d;
  document.getElementById('sub').textContent=`IP: ${d.ip} | ${d.fps} FPS | UDP Pkts: ${d.pkts}`;
  const mb=document.getElementById('modeBadge');
  mb.textContent=d.live?'LIVE UDP 11988':'WAITING FOR AUDIO';
  mb.className='badge'+(d.live?' live':'');
  document.getElementById('hz').textContent=Math.round(d.hz);
  document.getElementById('sx').value=d.sx;document.getElementById('sxV').textContent=d.sx;
  document.getElementById('ix').value=d.ix;document.getElementById('ixV').textContent=d.ix;
  document.getElementById('gain').value=d.gain;document.getElementById('gnV').textContent=d.gain+'%';
  document.getElementById('smooth').value=d.smooth;document.getElementById('smV').textContent=d.smooth;
  document.getElementById('bri').value=d.bri;document.getElementById('brV').textContent=d.bri;
  document.getElementById('btnPwr').className='t-btn'+(d.on?' on':'');
  document.getElementById('btnAuto').className='t-btn'+(d.auto?' on':'');
  document.getElementById('btnHide').className='t-btn'+(d.hide_ui?' on':'');
  document.getElementById('btnInv').className='t-btn'+(d.inv?' on':'');
  document.getElementById('btnSync').className='t-btn'+(d.sync_tuub?' on':'');
  for(let i=0;i<FX.length;i++){
    document.getElementById('fx'+i).className='fx-btn'+(d.fx===i?' active':'');
  }
  const bars=fftEl.children;
  for(let i=0;i<16;i++){bars[i].style.height=Math.round(d.fft[i]/255*64)+'px';}
  document.getElementById('tuubStat').textContent=d.tuub_ok?`ONLINE | FX #${d.tuub_fx} | Power: ${d.tuub_on?'ON':'OFF'}`:'OFFLINE (192.168.1.100)';
  document.getElementById('tBri').value=d.tuub_bri;document.getElementById('tBrV').textContent=d.tuub_bri;
  document.getElementById('tSx').value=d.tuub_sx;document.getElementById('tSxV').textContent=d.tuub_sx;
  document.getElementById('tIx').value=d.tuub_ix;document.getElementById('tIxV').textContent=d.tuub_ix;
}
setInterval(()=>fetch('/api/state').then(r=>r.json()).then(render).catch(()=>{}),400);
fetch('/api/state').then(r=>r.json()).then(render);
</script>
</body>
</html>)rawliteral";

static esp_err_t indexHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t apiStateHandler(httpd_req_t* req) {
    char query[192];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[32];
        if (httpd_query_key_value(query, "fx", val, sizeof(val)) == ESP_OK) {
            int v = atoi(val);
            if (v >= 0 && v < FX_COUNT) {
                gCfg.fx = (uint8_t)v;
                gCfg.autoCycle = false; // Lock when user explicitly selects an effect
            }
        }
        if (httpd_query_key_value(query, "sx", val, sizeof(val)) == ESP_OK) gCfg.sx = (uint8_t)atoi(val);
        if (httpd_query_key_value(query, "ix", val, sizeof(val)) == ESP_OK) gCfg.ix = (uint8_t)atoi(val);
        if (httpd_query_key_value(query, "bri", val, sizeof(val)) == ESP_OK) gCfg.bri = (uint8_t)atoi(val);
        if (httpd_query_key_value(query, "gain", val, sizeof(val)) == ESP_OK) gCfg.gainPct = (uint8_t)atoi(val);
        if (httpd_query_key_value(query, "smooth", val, sizeof(val)) == ESP_OK) gCfg.smooth = (uint8_t)atoi(val);
        if (httpd_query_key_value(query, "on", val, sizeof(val)) == ESP_OK) gCfg.powerOn = atoi(val) != 0;
        if (httpd_query_key_value(query, "auto", val, sizeof(val)) == ESP_OK) gCfg.autoCycle = atoi(val) != 0;
        if (httpd_query_key_value(query, "hide_ui", val, sizeof(val)) == ESP_OK) gCfg.hideUi = atoi(val) != 0;
        if (httpd_query_key_value(query, "inv", val, sizeof(val)) == ESP_OK) gCfg.invertDisp = atoi(val) != 0;
        if (httpd_query_key_value(query, "sync_tuub", val, sizeof(val)) == ESP_OK) gCfg.syncFromTuub = atoi(val) != 0;

        // Remote controls forwarded to ESP8266 "Tuub layt" (192.168.1.100)
        if (httpd_query_key_value(query, "tuub_bri", val, sizeof(val)) == ESP_OK) {
            gCfg.tuubBri = (uint8_t)atoi(val);
            snprintf(gCfg.pendingTuubCmd, sizeof(gCfg.pendingTuubCmd), "&A=%d", gCfg.tuubBri);
        }
        if (httpd_query_key_value(query, "tuub_sx", val, sizeof(val)) == ESP_OK) {
            gCfg.tuubSx = (uint8_t)atoi(val);
            snprintf(gCfg.pendingTuubCmd, sizeof(gCfg.pendingTuubCmd), "&SX=%d", gCfg.tuubSx);
        }
        if (httpd_query_key_value(query, "tuub_ix", val, sizeof(val)) == ESP_OK) {
            gCfg.tuubIx = (uint8_t)atoi(val);
            snprintf(gCfg.pendingTuubCmd, sizeof(gCfg.pendingTuubCmd), "&IX=%d", gCfg.tuubIx);
        }
        if (httpd_query_key_value(query, "tuub_on", val, sizeof(val)) == ESP_OK) {
            snprintf(gCfg.pendingTuubCmd, sizeof(gCfg.pendingTuubCmd), "&T=2");
        }
    }

    char json[680];
    snprintf(json, sizeof(json),
        "{\"on\":%s,\"bri\":%d,\"fx\":%d,\"sx\":%d,\"ix\":%d,\"gain\":%d,\"smooth\":%d,"
        "\"auto\":%s,\"hide_ui\":%s,\"inv\":%s,\"sync_tuub\":%s,\"live\":%s,\"hz\":%.1f,"
        "\"fps\":%d,\"pkts\":%lu,\"ip\":\"%s\","
        "\"tuub_ok\":%s,\"tuub_on\":%s,\"tuub_bri\":%d,\"tuub_fx\":%d,\"tuub_sx\":%d,\"tuub_ix\":%d,"
        "\"fft\":[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]}",
        gCfg.powerOn ? "true" : "false", gCfg.bri, gCfg.fx, gCfg.sx, gCfg.ix, gCfg.gainPct, gCfg.smooth,
        gCfg.autoCycle ? "true" : "false", gCfg.hideUi ? "true" : "false",
        gCfg.invertDisp ? "true" : "false",
        gCfg.syncFromTuub ? "true" : "false", gAudio.liveAudio ? "true" : "false",
        gAudio.dominantHz, gCurrentFps, (unsigned long)gAudio.udpPackets, gCfg.ipStr,
        gCfg.tuubOnline ? "true" : "false", gCfg.tuubOn ? "true" : "false",
        gCfg.tuubBri, gCfg.tuubFx, gCfg.tuubSx, gCfg.tuubIx,
        gAudio.bins[0], gAudio.bins[1], gAudio.bins[2], gAudio.bins[3],
        gAudio.bins[4], gAudio.bins[5], gAudio.bins[6], gAudio.bins[7],
        gAudio.bins[8], gAudio.bins[9], gAudio.bins[10], gAudio.bins[11],
        gAudio.bins[12], gAudio.bins[13], gAudio.bins[14], gAudio.bins[15]);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t winCompatHandler(httpd_req_t* req) {
    char query[128];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[24];
        if (httpd_query_key_value(query, "A", val, sizeof(val)) == ESP_OK) gCfg.bri = (uint8_t)atoi(val);
        if (httpd_query_key_value(query, "FX", val, sizeof(val)) == ESP_OK) gCfg.fx = (uint8_t)(atoi(val) % FX_COUNT);
        if (httpd_query_key_value(query, "SX", val, sizeof(val)) == ESP_OK) gCfg.sx = (uint8_t)atoi(val);
        if (httpd_query_key_value(query, "IX", val, sizeof(val)) == ESP_OK) gCfg.ix = (uint8_t)atoi(val);
    }
    char xml[256];
    snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\" ?><vs><ac>%d</ac><fx>%d</fx><sx>%d</sx><ix>%d</ix><ds>O-LED 128x64</ds></vs>",
        gCfg.powerOn ? gCfg.bri : 0, gCfg.fx, gCfg.sx, gCfg.ix);
    httpd_resp_set_type(req, "text/xml");
    return httpd_resp_send(req, xml, HTTPD_RESP_USE_STRLEN);
}

static void startWebServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 6144;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uRoot = { "/", HTTP_GET, indexHandler, NULL };
        httpd_uri_t uApi  = { "/api/state", HTTP_GET, apiStateHandler, NULL };
        httpd_uri_t uWin  = { "/win", HTTP_GET, winCompatHandler, NULL };
        httpd_register_uri_handler(server, &uRoot);
        httpd_register_uri_handler(server, &uApi);
        httpd_register_uri_handler(server, &uWin);
        printf("[HTTP] O-LED Web UI started at http://%s/\n", gCfg.ipStr);
    }
}

static void wifiEventHandler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        gCfg.wifiConnected = false;
        snprintf(gCfg.ipStr, sizeof(gCfg.ipStr), "RECONNECTING");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)data;
        snprintf(gCfg.ipStr, sizeof(gCfg.ipStr), IPSTR, IP2STR(&event->ip_info.ip));
        gCfg.wifiConnected = true;
        gCfg.ipShowUntilMs = millis() + 12000;
        printf("[WIFI] Connected! O-LED IP: %s (Web UI: http://%s/)\n", gCfg.ipStr, gCfg.ipStr);
    }
}

inline void initWledNetwork() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    loadConfigFromNvs();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t* staNetif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(staNetif, "o-led");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, NULL, NULL);

    wifi_config_t wifiCfg = {};
    strncpy((char*)wifiCfg.sta.ssid, WIFI_SSID_DEFAULT, sizeof(wifiCfg.sta.ssid) - 1);
    strncpy((char*)wifiCfg.sta.password, WIFI_PASS_DEFAULT, sizeof(wifiCfg.sta.password) - 1);
    wifiCfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifiCfg);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE);

    startWebServer();
    xTaskCreatePinnedToCore(udpAudioTask, "udp_audio", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(tuubTwinTask, "tuub_twin", 4096, NULL, 3, NULL, 0);
}

// Updates gAudio each frame: ambient effect time runs continuously, while audio values
// come strictly from live UDP 11988 packets and are filtered by gCfg.smooth (0..255).
inline void updateAudioReactiveState(uint32_t nowMs) {
    float speedScale = 0.35f + (gCfg.sx * (1.0f / 255.0f)) * 1.3f;
    gAudio.timeSec = (nowMs * 0.001f) * speedScale;
    gAudio.totalBeats = gAudio.timeSec * 2.0f;
    gAudio.beatIndex = (uint32_t)floorf(gAudio.totalBeats);
    gAudio.barIndex = gAudio.beatIndex / 4;
    gAudio.beatPhase = gAudio.totalBeats - (float)gAudio.beatIndex;

    // Asymmetric Attack/Release EMA smoothing factors from gCfg.smooth (0..255)
    float sNorm = gCfg.smooth * (1.0f / 255.0f);
    float attackAlpha  = 1.0f - sNorm * 0.72f; // 1.00 (raw) .. 0.28 (fast punchy attack)
    float releaseAlpha = 1.0f - sNorm * 0.91f; // 1.00 (raw) .. 0.09 (liquid smooth decay)

    // If no active UDP audio packet arrived within last 350 ms, zero targets and decay
    if (gAudio.lastUdpMs == 0 || (nowMs - gAudio.lastUdpMs > 350)) {
        gAudio.liveAudio = false;
        gAudio.targetBass = 0.0f;
        gAudio.targetMid  = 0.0f;
        gAudio.targetHigh = 0.0f;
        gAudio.targetHz   = 0.0f;
        gAudio.samplePeak = 0;
        for (int i = 0; i < 16; i++) {
            gAudio.targetBins[i] = 0;
        }
        gAudio.kickEnv *= 0.75f;
        if (gAudio.kickEnv < 0.01f) gAudio.kickEnv = 0.0f;
        gAudio.snareEnv *= 0.75f;
        if (gAudio.snareEnv < 0.01f) gAudio.snareEnv = 0.0f;
    } else {
        // Decay transient envelopes smoothly between live UDP packets
        float kickDecay = 0.82f + sNorm * 0.12f;
        gAudio.kickEnv *= kickDecay;
        gAudio.snareEnv *= kickDecay;
    }

    // Smoothly interpolate all 16 FFT bands & envelopes using Attack/Release filter
    for (int i = 0; i < 16; i++) {
        float tgt = (float)gAudio.targetBins[i];
        float cur = gAudio.smoothBins[i];
        float a = (tgt > cur) ? attackAlpha : releaseAlpha;
        cur += (tgt - cur) * a;
        if (cur < 0.4f) cur = 0.0f;
        gAudio.smoothBins[i] = cur;
        gAudio.bins[i] = (uint8_t)(cur + 0.5f);
    }

    auto smoothEnv = [&](float& cur, float tgt) {
        float a = (tgt > cur) ? attackAlpha : releaseAlpha;
        cur += (tgt - cur) * a;
        if (cur < 0.005f) cur = 0.0f;
    };
    smoothEnv(gAudio.bassEnv, gAudio.targetBass);
    smoothEnv(gAudio.midEnv,  gAudio.targetMid);
    smoothEnv(gAudio.highEnv, gAudio.targetHigh);

    if (!gAudio.liveAudio) {
        gAudio.dominantHz = 0.0f;
    } else {
        gAudio.dominantHz += (gAudio.targetHz - gAudio.dominantHz) * attackAlpha;
    }

    // Gravity decay on 16-band peak-hold caps
    for (int i = 0; i < 16; i++) {
        if (gAudio.peakHold[i] > 4) gAudio.peakHold[i] -= 4;
        else gAudio.peakHold[i] = 0;
    }
}
