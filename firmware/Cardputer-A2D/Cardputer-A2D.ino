#include "secrets.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <M5Cardputer.h>
#include "time.h"

// ══════════════════════════════════════════════════════════════════════════════
// Credenciais do painel web (alvo de brute force)
// ══════════════════════════════════════════════════════════════════════════════
#define WEB_USER "MLO"
#define WEB_PASS "admin"

// ══════════════════════════════════════════════════════════════════════════════
// HTML — login panel 
// ══════════════════════════════════════════════════════════════════════════════
static const char LOGIN_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CloudCard · Login</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@600&display=swap');
  :root{--bg:#030a05;--green:#00e676;--green2:#69f0ae;--red:#ff5252;--cyan:#40c4ff;--text:#b9f6ca;--muted:#4caf50;--border:1px solid #1b5e20}
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Share Tech Mono',monospace;
       min-height:100vh;display:flex;align-items:center;justify-content:center;
       background-image:repeating-linear-gradient(0deg,transparent,transparent 23px,rgba(0,230,118,.03) 24px)}
  body::after{content:'';position:fixed;inset:0;pointer-events:none;
              background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,0,0,.07) 2px,rgba(0,0,0,.07) 4px);z-index:100}
  .box{background:#081008;border:var(--border);border-radius:6px;padding:36px 32px;
       width:100%;max-width:380px;box-shadow:0 0 40px rgba(0,230,118,.08)}
  h1{font-family:'Orbitron',monospace;font-size:1rem;color:var(--green);
     letter-spacing:.2em;text-shadow:0 0 10px var(--green);margin-bottom:4px}
  .sub{font-size:.65rem;color:var(--muted);margin-bottom:28px}
  label{font-size:.72rem;color:var(--muted);display:block;margin-bottom:6px}
  input{width:100%;background:#0d1a0d;border:var(--border);border-radius:3px;
        color:var(--text);font-family:'Share Tech Mono',monospace;font-size:.85rem;
        padding:10px 12px;outline:none;transition:.15s;margin-bottom:16px}
  input:focus{border-color:var(--green);box-shadow:0 0 8px rgba(0,230,118,.2)}
  button{width:100%;background:rgba(0,230,118,.1);border:1px solid var(--green);
         border-radius:3px;color:var(--green);font-family:'Orbitron',monospace;
         font-size:.8rem;letter-spacing:.1em;padding:12px;cursor:pointer;transition:.15s}
  button:hover{background:rgba(0,230,118,.2);box-shadow:0 0 12px rgba(0,230,118,.3)}
  .msg{margin-top:16px;padding:10px;border-radius:3px;font-size:.75rem;text-align:center;display:none}
  .msg-err{background:rgba(255,82,82,.1);border:1px solid var(--red);color:var(--red)}
  .msg-ok{background:rgba(0,230,118,.1);border:1px solid var(--green);color:var(--green)}
  .attempts{font-size:.65rem;color:var(--muted);text-align:right;margin-top:12px}
</style>
</head>
<body>
<div class="box">
  <h1>&#9632; CLOUDCARD</h1>
  <div class="sub">IoT Lab · Painel de Controle</div>
  <form id="frm">
    <label>Usuário</label>
    <input type="text" id="u" name="username" autocomplete="off" placeholder="admin">
    <label>Senha</label>
    <input type="password" id="p" name="password" placeholder="••••••••">
    <button type="submit">&#9654; ENTRAR</button>
  </form>
  <div class="msg" id="msg"></div>
  <div class="attempts" id="att"></div>
</div>
<script>
let attempts=0;
document.getElementById('frm').addEventListener('submit',function(e){
  e.preventDefault();
  const u=document.getElementById('u').value;
  const pw=document.getElementById('p').value;
  attempts++;
  fetch('/api/login',{
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'u='+encodeURIComponent(u)+'&p='+encodeURIComponent(pw)
  }).then(r=>r.json()).then(d=>{
    const m=document.getElementById('msg');
    m.style.display='block';
    if(d.ok){
      m.className='msg msg-ok';
      m.textContent='✓ Acesso autorizado. Redirecionando...';
      setTimeout(()=>window.location='/dashboard',1500);
    } else {
      m.className='msg msg-err';
      m.textContent='✗ Credenciais inválidas (tentativa '+attempts+')';
    }
    document.getElementById('att').textContent='Tentativas: '+attempts;
  }).catch(()=>{});
});
</script>
</body></html>
)HTML";

// ── Dashboard (pós-login) ─────────────────────────────────────────────────────
static const char DASH_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CloudCard · Dashboard</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@600&display=swap');
  :root{--bg:#030a05;--bg2:#081008;--green:#00e676;--green2:#69f0ae;--red:#ff5252;--cyan:#40c4ff;--orange:#ffab40;--text:#b9f6ca;--muted:#4caf50;--border:1px solid #1b5e20}
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Share Tech Mono',monospace;min-height:100vh;padding:20px;
       background-image:repeating-linear-gradient(0deg,transparent,transparent 23px,rgba(0,230,118,.03) 24px)}
  body::after{content:'';position:fixed;inset:0;pointer-events:none;background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,0,0,.05) 2px,rgba(0,0,0,.05) 4px);z-index:100}
  .header{display:flex;justify-content:space-between;align-items:center;border-bottom:var(--border);padding-bottom:12px;margin-bottom:20px}
  h1{font-family:'Orbitron',monospace;font-size:1rem;color:var(--green);letter-spacing:.15em;text-shadow:0 0 10px var(--green)}
  .tag{font-size:.65rem;color:var(--muted)}
  .ip{color:var(--cyan);font-size:.8rem}
  .grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:16px}
  .card{background:var(--bg2);border:var(--border);border-radius:4px;padding:14px}
  .card-title{font-size:.65rem;color:var(--muted);text-transform:uppercase;letter-spacing:.08em;margin-bottom:8px}
  .card-val{font-family:'Orbitron',monospace;font-size:1.6rem;color:var(--green2)}
  .card-sub{font-size:.65rem;color:var(--muted);margin-top:4px}
  .dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px}
  .dot-on{background:var(--green);box-shadow:0 0 6px var(--green)}
  .dot-warn{background:var(--orange);box-shadow:0 0 6px var(--orange)}
  .mode-bar{background:var(--bg2);border:var(--border);border-radius:4px;padding:12px 16px;
            display:flex;align-items:center;gap:12px;margin-bottom:16px}
  .badge{padding:3px 10px;border-radius:2px;font-family:'Orbitron',monospace;font-size:.65rem;letter-spacing:.05em}
  .badge-normal{background:rgba(0,230,118,.15);color:var(--green);border:1px solid var(--green)}
  .badge-flood{background:rgba(255,82,82,.15);color:var(--red);border:1px solid var(--red)}
  .mode-name{font-family:'Orbitron',monospace;font-size:.85rem;color:var(--green2)}
  .rate{font-size:.7rem;color:var(--muted);margin-left:auto}
  .btns{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:16px}
  .btn{background:#0d1a0d;border:var(--border);border-radius:4px;color:var(--text);
       font-family:'Share Tech Mono',monospace;font-size:.85rem;padding:14px;
       cursor:pointer;text-align:center;transition:.15s}
  .btn:hover{border-color:var(--green);background:rgba(0,230,118,.08)}
  .btn.active{border-color:var(--green);color:var(--green);background:rgba(0,230,118,.1);box-shadow:0 0 10px rgba(0,230,118,.2)}
  .btn-flood{color:var(--red)}
  .btn-flood.active{border-color:var(--red);color:var(--red);background:rgba(255,82,82,.1)}
  .payload-box{background:var(--bg2);border:var(--border);border-radius:4px;padding:12px;
               font-size:.7rem;line-height:1.6;white-space:pre;overflow-x:auto;color:var(--green2);min-height:70px}
  .section{font-size:.65rem;color:var(--muted);text-transform:uppercase;letter-spacing:.1em;margin-bottom:8px}
  .logout{font-size:.7rem;color:var(--muted);cursor:pointer;text-decoration:underline}
  .logout:hover{color:var(--red)}
</style>
</head>
<body>
<div class="header">
  <div><h1>&#9632; CLOUDCARD</h1><div class="tag">IoT Sensor Dashboard</div></div>
  <div style="text-align:right">
    <div class="ip" id="devip">---</div>
    <div class="logout" onclick="fetch('/api/logout').then(()=>location.href='/')">logout</div>
  </div>
</div>

<div class="grid">
  <div class="card">
    <div class="card-title">&#9632; Temperatura</div>
    <div class="card-val" id="val-t">--.-</div>
    <div class="card-sub">°C</div>
  </div>
  <div class="card">
    <div class="card-title">&#9632; Umidade</div>
    <div class="card-val" id="val-h">--.-</div>
    <div class="card-sub">%</div>
  </div>
  <div class="card">
    <div class="card-title">&#9632; Pressão</div>
    <div class="card-val" id="val-p" style="font-size:1.2rem">----.-</div>
    <div class="card-sub">hPa</div>
  </div>
  <div class="card">
    <div class="card-title">&#9632; RSSI Wi-Fi</div>
    <div class="card-val" id="val-rssi">---</div>
    <div class="card-sub">dBm</div>
  </div>
</div>

<div class="mode-bar">
  <span class="badge" id="mode-badge">NORMAL</span>
  <span class="mode-name" id="mode-name">TELEMETRIA</span>
  <span class="rate" id="mode-rate">4000 ms / pkt</span>
</div>

<div class="section">&#9654; Controle de modo</div>
<div class="btns">
  <div class="btn active" id="btn-normal" onclick="setMode('normal')">&#9632; NORMAL<br><small style="color:var(--muted);font-size:.65rem">4s · QoS 1</small></div>
  <div class="btn btn-flood" id="btn-flood" onclick="setMode('flood')">&#9621; FLOOD<br><small style="color:var(--muted);font-size:.65rem">50ms · QoS 0</small></div>
</div>

<div class="grid" style="margin-bottom:16px">
  <div class="card" style="text-align:center">
    <div class="card-title">Normal pkts</div>
    <div class="card-val" id="cnt-n" style="color:var(--green)">0</div>
  </div>
  <div class="card" style="text-align:center">
    <div class="card-title">Flood pkts</div>
    <div class="card-val" id="cnt-f" style="color:var(--red)">0</div>
  </div>
</div>

<div class="section">&#9632; Último payload MQTT</div>
<div class="payload-box" id="last-payload">aguardando...</div>

<script>
function setMode(m){
  fetch('/api/mode?m='+m).then(r=>r.json()).then(poll);
}
function poll(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    if(d.auth===false){location.href='/';return;}
    document.getElementById('devip').textContent=d.ip;
    document.getElementById('val-t').textContent=parseFloat(d.t||0).toFixed(1);
    document.getElementById('val-h').textContent=parseFloat(d.h||0).toFixed(1);
    document.getElementById('val-p').textContent=parseFloat(d.p||0).toFixed(1);
    document.getElementById('val-rssi').textContent=d.rssi||'--';
    document.getElementById('cnt-n').textContent=d.normal;
    document.getElementById('cnt-f').textContent=d.flood;
    // mode
    const isFlood=d.mode==='flood';
    document.getElementById('mode-badge').textContent=isFlood?'FLOOD':'NORMAL';
    document.getElementById('mode-badge').className='badge '+(isFlood?'badge-flood':'badge-normal');
    document.getElementById('mode-name').textContent=isFlood?'SYN FLOOD':'TELEMETRIA';
    document.getElementById('mode-rate').textContent=isFlood?'50 ms / pkt':'4000 ms / pkt';
    document.getElementById('btn-normal').className='btn'+(isFlood?'':' active');
    document.getElementById('btn-flood').className='btn btn-flood'+(isFlood?' active':'');
    if(d.last)document.getElementById('last-payload').textContent=d.last;
  }).catch(()=>{});
}
setInterval(poll,2000);poll();
</script>
</body></html>
)HTML";

// ══════════════════════════════════════════════════════════════════════════════
// Estado global
// ══════════════════════════════════════════════════════════════════════════════
enum SimMode { MODE_NORMAL = 0, MODE_FLOOD };

WiFiClientSecure net;
MQTTClient       mqtt(2048);
WebServer        webServer(WEBUI_PORT);
M5Canvas         canvas(&M5Cardputer.Display);

SimMode       mode         = MODE_NORMAL;
bool          webLoggedIn  = false;   // sessão web simples

uint32_t      cntNormal = 0, cntFlood = 0, cntTotal = 0;
unsigned long lastNormal = 0, lastFlood = 0;

// Sensor drift
float tempBase  = 24.0f, humBase = 60.0f, pressBase = 1013.0f;

// ── Buffers fixos — zero heap em runtime ─────────────────────────────────────
static char _tBuf[10], _hBuf[10], _pBuf[10];
static char _jsonBuf[200];
static char _uiL1[32], _uiL2[32];
static char _ipBuf[20];
static char _lastPayload[200] = "---";

// UI
static char uiTitle[24]  = "CloudCard";
static char uiMsg[32]    = "Iniciando...";
static char uiLine3[32]  = "";
static int  uiTitleColor = TFT_CYAN;
static bool uiDirty      = true;
static char inputBuffer[32] = "";
static uint8_t inputLen  = 0;

// Docs JSON globais — alocados uma vez
StaticJsonDocument<200> sensorDoc;
StaticJsonDocument<260> statusDoc;

// ══════════════════════════════════════════════════════════════════════════════
// Utilitários
// ══════════════════════════════════════════════════════════════════════════════
time_t getEpoch() { time_t t; time(&t); return t; }

float frand(float lo, float hi) {
  return lo + (random(0, 10000) / 10000.0f) * (hi - lo);
}

const char* modeStr() { return mode == MODE_FLOOD ? "flood" : "normal"; }

void setUI(const char* title, const char* msg, const char* line3, int color) {
  strlcpy(uiTitle,  title,  sizeof(uiTitle));
  strlcpy(uiMsg,    msg,    sizeof(uiMsg));
  strlcpy(uiLine3,  line3,  sizeof(uiLine3));
  uiTitleColor = color;
  uiDirty      = true;
}

// ══════════════════════════════════════════════════════════════════════════════
// Display
// ══════════════════════════════════════════════════════════════════════════════
void renderDisplay() {
  if (!uiDirty) return;
  uiDirty = false;

  canvas.fillScreen(TFT_BLACK);

  // Header
  canvas.fillRect(0, 0, 240, 22, uiTitleColor);
  canvas.setTextColor(TFT_BLACK);
  canvas.setTextSize(1);
  canvas.setCursor(5, 7);
  canvas.print(uiTitle);
  canvas.setCursor(160, 7);
  canvas.print(_ipBuf[0] ? _ipBuf : "no wifi");

  // Body
  canvas.setTextColor(TFT_WHITE);
  canvas.setCursor(5, 28);
  canvas.print(uiMsg);
  canvas.setCursor(5, 42);
  canvas.setTextColor(TFT_CYAN);
  canvas.print(uiLine3);

  // Counters
  canvas.fillRect(0, 58, 240, 14, 0x0820);
  canvas.setTextColor(TFT_GREEN);
  canvas.setCursor(4, 61);
  canvas.printf("N:%-5lu F:%-5lu T:%lu", cntNormal, cntFlood, cntTotal);

  // Clock
  time_t now = getEpoch();
  struct tm* ti = gmtime(&now);
  int brHour = (ti->tm_hour - 3 + 24) % 24;
  canvas.setTextColor(0x7BEF);
  canvas.setCursor(178, 61);
  canvas.printf("%02d:%02d", brHour, ti->tm_min);

  // Input bar
  canvas.fillRect(0, 74, 240, 15, 0x2104);
  canvas.setTextColor(TFT_WHITE);
  canvas.setCursor(4, 77);
  canvas.print("> ");
  canvas.print(inputBuffer);
  if (millis() % 900 < 450) canvas.print("|");

  // Mode indicator bottom bar
  canvas.fillRect(0, 90, 240, 10, mode == MODE_FLOOD ? 0x6000 : 0x0200);
  canvas.setTextColor(TFT_WHITE);
  canvas.setCursor(4, 92);
  canvas.setTextSize(1);
  canvas.print(mode == MODE_FLOOD
    ? "FLOOD  ~20pkt/s  QoS0  payload=normal"
    : "NORMAL  4s/pkt  QoS1  /flood para ativar");

  // Web panel info
  canvas.fillRect(0, 102, 240, 33, 0x0010);
  canvas.setTextColor(0x5AEB);
  canvas.setCursor(4, 105);
  canvas.printf("WEB: http://%s", _ipBuf);
  canvas.setCursor(4, 115);
  canvas.setTextColor(webLoggedIn ? TFT_GREEN : TFT_ORANGE);
  canvas.print(webLoggedIn ? "Login: AUTENTICADO" : "Login: aguardando...");
  canvas.setCursor(4, 125);
  canvas.setTextColor(0x5AEB);
  canvas.printf("Brute: admin / ???");

  canvas.pushSprite(0, 0);
}

// ══════════════════════════════════════════════════════════════════════════════
// Sensor drift — compartilhado entre normal e flood
// ══════════════════════════════════════════════════════════════════════════════
void sensorDrift() {
  tempBase  = constrain(tempBase  + frand(-0.25f, 0.25f), 18.0f, 38.0f);
  humBase   = constrain(humBase   + frand(-0.40f, 0.40f), 30.0f, 90.0f);
  pressBase = constrain(pressBase + frand(-0.15f, 0.15f), 995.0f, 1030.0f);
  dtostrf(tempBase,  5, 2, _tBuf);
  dtostrf(humBase,   5, 2, _hBuf);
  dtostrf(pressBase, 7, 1, _pBuf);
}

// ── buildSensorDoc — monta o JSON de sensor (mesmo para normal e flood) ───────
void buildSensorDoc(uint32_t seq) {
  sensorDoc.clear();
  sensorDoc["d"]    = DEVICE_ID;
  sensorDoc["ts"]   = (uint32_t)getEpoch();
  sensorDoc["t"]    = _tBuf;
  sensorDoc["h"]    = _hBuf;
  sensorDoc["p"]    = _pBuf;
  sensorDoc["rssi"] = (int8_t)WiFi.RSSI();
  sensorDoc["seq"]  = seq;
  serializeJson(sensorDoc, _jsonBuf, sizeof(_jsonBuf));
}

// ══════════════════════════════════════════════════════════════════════════════
// publishNormal — QoS 1, 4s, payload telemetria
// ══════════════════════════════════════════════════════════════════════════════
void publishNormal() {
  sensorDrift();
  buildSensorDoc(cntNormal + 1);

  if (mqtt.publish(TOPIC_NORMAL, _jsonBuf, false, 1)) {
    cntNormal++; cntTotal++;
    strlcpy(_lastPayload, _jsonBuf, sizeof(_lastPayload));
    snprintf(_uiL1, sizeof(_uiL1), "T:%.1f C  H:%.1f%%", tempBase, humBase);
    snprintf(_uiL2, sizeof(_uiL2), "P:%.1fhPa seq:%lu", pressBase, cntNormal);
    setUI("NORMAL", _uiL1, _uiL2, TFT_DARKGREEN);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// publishFlood — QoS 0, 50ms, payload IDÊNTICO ao normal
// A única diferença detectável é o volume/frequência no PCAP
// ══════════════════════════════════════════════════════════════════════════════
void publishFlood() {
  if (!mqtt.connected()) return;

  sensorDrift();
  buildSensorDoc(cntTotal);   // seq contínua — não denuncia o modo

  if (mqtt.publish(TOPIC_NORMAL, _jsonBuf, false, 0)) {
    cntFlood++; cntTotal++;
    if (cntFlood % 50 == 0) {
      strlcpy(_lastPayload, _jsonBuf, sizeof(_lastPayload));
      snprintf(_uiL1, sizeof(_uiL1), "T:%.1f C  H:%.1f%%", tempBase, humBase);
      snprintf(_uiL2, sizeof(_uiL2), "~20pkt/s  seq:%lu", cntTotal);
      setUI("[ FLOOD ]", _uiL1, _uiL2, TFT_RED);
    }
  }

  yield();   // feed watchdog
}

// ══════════════════════════════════════════════════════════════════════════════
// applyMode
// ══════════════════════════════════════════════════════════════════════════════
void applyMode(const char* cmd) {
  if (!strncmp(cmd, "normal", 6) || !strncmp(cmd, "stop", 4)) {
    mode = MODE_NORMAL;
    setUI("NORMAL", "Telemetria ativa", "4s por pacote QoS1", TFT_DARKGREEN);
    M5Cardputer.Speaker.tone(1800, 50);
  } else if (!strncmp(cmd, "flood", 5)) {
    mode = MODE_FLOOD;
    cntFlood = 0;
    setUI("[ FLOOD ]", "Flood ativo", "50ms  payload=normal", TFT_RED);
    M5Cardputer.Speaker.tone(300, 250);
  } else if (!strncmp(cmd, "status", 6)) {
    snprintf(_uiL1, sizeof(_uiL1), "N:%lu F:%lu", cntNormal, cntFlood);
    setUI("STATUS", _uiL1, modeStr(), TFT_CYAN);
  } else if (!strncmp(cmd, "help", 4)) {
    setUI("HELP", "/normal  /flood", "/stop  /status", TFT_CYAN);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// Web Server — login + dashboard + api
// ══════════════════════════════════════════════════════════════════════════════
static uint32_t loginAttempts = 0;
static uint32_t loginSuccess  = 0;

void setupWebRoutes() {

  // ── GET / → login page ────────────────────────────────────────────────────
  webServer.on("/", HTTP_GET, []() {
    webServer.send_P(200, "text/html", LOGIN_HTML);
  });

  // ── GET /dashboard → só com sessão ───────────────────────────────────────
  webServer.on("/dashboard", HTTP_GET, []() {
    if (!webLoggedIn) {
      webServer.sendHeader("Location", "/");
      webServer.send(302);
      return;
    }
    webServer.send_P(200, "text/html", DASH_HTML);
  });

  // ── POST /api/login ───────────────────────────────────────────────────────
  webServer.on("/api/login", HTTP_POST, []() {
    loginAttempts++;

    bool ok = false;
    if (webServer.hasArg("u") && webServer.hasArg("p")) {
      String u = webServer.arg("u");
      String p = webServer.arg("p");
      ok = (u == WEB_USER && p == WEB_PASS);
    }

    if (ok) {
      webLoggedIn = true;
      loginSuccess++;
      // Mostra no display
      snprintf(_uiL1, sizeof(_uiL1), "LOGIN OK #%lu", loginSuccess);
      setUI("WEB AUTH", _uiL1, "admin autenticado", TFT_GREEN);
      M5Cardputer.Speaker.tone(2000, 80);
    } else {
      // Mostra tentativa falha no display
      snprintf(_uiL1, sizeof(_uiL1), "FALHA #%lu", loginAttempts);
      setUI("BRUTE?", _uiL1, "credencial errada", TFT_ORANGE);
    }

    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.send(200, "application/json",
      ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  // ── GET /api/logout ───────────────────────────────────────────────────────
  webServer.on("/api/logout", HTTP_GET, []() {
    webLoggedIn = false;
    setUI("NORMAL", "Logout realizado", "sessao encerrada", TFT_DARKGREEN);
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.send(200, "application/json", "{\"ok\":true}");
  });

  // ── GET /api/status ───────────────────────────────────────────────────────
  webServer.on("/api/status", HTTP_GET, []() {
    if (!webLoggedIn) {
      webServer.send(200, "application/json", "{\"auth\":false}");
      return;
    }
    statusDoc.clear();
    statusDoc["auth"]   = true;
    statusDoc["ip"]     = _ipBuf;
    statusDoc["wifi"]   = (WiFi.status() == WL_CONNECTED);
    statusDoc["mqtt"]   = mqtt.connected();
    statusDoc["mode"]   = modeStr();
    statusDoc["normal"] = cntNormal;
    statusDoc["flood"]  = cntFlood;
    statusDoc["total"]  = cntTotal;
    statusDoc["t"]      = _tBuf;
    statusDoc["h"]      = _hBuf;
    statusDoc["p"]      = _pBuf;
    statusDoc["rssi"]   = (int8_t)WiFi.RSSI();
    statusDoc["last"]   = _lastPayload;

    static char sBuf[260];
    serializeJson(statusDoc, sBuf, sizeof(sBuf));
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.send(200, "application/json", sBuf);
  });

  // ── GET /api/mode ─────────────────────────────────────────────────────────
  webServer.on("/api/mode", HTTP_GET, []() {
    if (!webLoggedIn) {
      webServer.send(403, "application/json", "{\"err\":\"unauth\"}");
      return;
    }
    if (webServer.hasArg("m")) {
      static char modeBuf[16];
      webServer.arg("m").toCharArray(modeBuf, sizeof(modeBuf));
      applyMode(modeBuf);
    }
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.send(200, "application/json", "{\"ok\":true}");
  });

  webServer.onNotFound([]() {
    webServer.send(404, "text/plain", "not found");
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// MQTT handler
// ══════════════════════════════════════════════════════════════════════════════
void mqttHandler(String& topic, String& payload) {
  static char pl[40];
  payload.toCharArray(pl, sizeof(pl));
  setUI("BROKER MSG", pl, "", TFT_BLUE);
}

// ══════════════════════════════════════════════════════════════════════════════
// connectAll
// ══════════════════════════════════════════════════════════════════════════════
bool connectAll() {
  int att = 0;

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    setUI("Wi-Fi", "Conectando...", WIFI_SSID, TFT_ORANGE);
    renderDisplay();
    while (WiFi.status() != WL_CONNECTED && att < 40) { delay(250); att++; }
    if (WiFi.status() != WL_CONNECTED) {
      setUI("ERRO", "Wi-Fi falhou", "", TFT_RED);
      renderDisplay();
      return false;
    }
  }

  WiFi.localIP().toString().toCharArray(_ipBuf, sizeof(_ipBuf));

  // NTP
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  time_t now = 0; att = 0;
  while (now < 100000 && att++ < 60) { time(&now); delay(100); }

  // TLS
  if (String(BROKER_CA_CERT).indexOf("<COLE_AQUI") != -1) {
    net.setInsecure();
  } else {
    net.setCACert(BROKER_CA_CERT);
  }
  net.setCertificate(CLIENT_CERT);
  net.setPrivateKey(CLIENT_KEY);

  // MQTT
  mqtt.begin(MQTT_HOST, MQTT_PORT, net);
  mqtt.onMessage(mqttHandler);
  mqtt.setKeepAlive(30);

  setUI("MQTT", "Conectando...", MQTT_HOST, TFT_ORANGE);
  renderDisplay();

  att = 0;
  while (!mqtt.connect(MQTT_CLIENT) && att++ < 20) delay(300);

  if (!mqtt.connected()) {
    setUI("ERRO", "Broker falhou", "", TFT_RED);
    renderDisplay();
    return false;
  }

  mqtt.subscribe(TOPIC_NORMAL);
  return true;
}

// ══════════════════════════════════════════════════════════════════════════════
// Setup
// ══════════════════════════════════════════════════════════════════════════════
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  Serial.begin(115200);
  randomSeed(analogRead(0) ^ esp_random());

  canvas.createSprite(240, 135);
  setUI("CloudCard", "Iniciando...", "", TFT_CYAN);
  renderDisplay();
  delay(300);

  if (!connectAll()) { delay(5000); ESP.restart(); }

  setupWebRoutes();
  webServer.begin();

  snprintf(_uiL1, sizeof(_uiL1), "Online %s", _ipBuf);
  snprintf(_uiL2, sizeof(_uiL2), "Web: http://%s", _ipBuf);
  setUI("NORMAL", _uiL1, _uiL2, TFT_DARKGREEN);
  renderDisplay();
  M5Cardputer.Speaker.tone(2000, 60);
  delay(1000);
  setUI("NORMAL", "Telemetria ativa", "/flood  /normal  /help", TFT_DARKGREEN);
}

// ══════════════════════════════════════════════════════════════════════════════
// Loop
// ══════════════════════════════════════════════════════════════════════════════
void loop() {
  M5Cardputer.update();
  mqtt.loop();
  webServer.handleClient();

  // Reconexão MQTT
  if (!mqtt.connected()) {
    static unsigned long lastReconn = 0;
    if (millis() - lastReconn > 5000) {
      lastReconn = millis();
      setUI("RECONECT", "Broker...", "", TFT_ORANGE);
      uiDirty = true;
      connectAll();
    }
  }

  // ── Normal: sempre ativo em modo normal ──
  if (mode == MODE_NORMAL && millis() - lastNormal >= 4000) {
    lastNormal = millis();
    publishNormal();
  }

  // ── Flood: substitui o normal completamente ──
  if (mode == MODE_FLOOD && millis() - lastFlood >= 50) {
    lastFlood = millis();
    publishFlood();
  }

  // ── Teclado ──
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();

    if (ks.del && inputLen > 0) {
      inputBuffer[--inputLen] = '\0';
      uiDirty = true;
    }
    if (ks.enter && inputLen > 0) {
      applyMode(inputBuffer);
      inputBuffer[0] = '\0';
      inputLen = 0;
      uiDirty = true;
    }
    for (auto c : ks.word) {
      if (inputLen < sizeof(inputBuffer) - 1) {
        inputBuffer[inputLen++] = c;
        inputBuffer[inputLen]   = '\0';
        uiDirty = true;
      }
    }
  }

  // Botão A → volta para normal
  if (M5Cardputer.BtnA.wasPressed()) {
    applyMode("normal");
    inputBuffer[0] = '\0';
    inputLen = 0;
  }

  // Render
  renderDisplay();

  // Refresh relógio
  static unsigned long lastClock = 0;
  if (millis() - lastClock > 10000) {
    lastClock = millis();
    uiDirty = true;
  }
}