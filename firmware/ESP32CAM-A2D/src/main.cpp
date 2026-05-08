#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "esp_camera.h"
#include "img_converters.h"

// ─── Credenciais ─────────────────────────────────────────────────────────────
static const char* WIFI_SSID   = "SecurityWifi"; // SEU SSID
static const char* WIFI_PASS   = "123456789";    // SUA SENHA
static const char* MQTT_HOST   = "IP"; // BROKER
static const int   MQTT_PORT   = 8883; // SUA PORTA 883 para Tls e 1883 para não-TLS
static const char* MQTT_CLIENT = "espcam_01"; // SEU CLIENT ID
static const char* MQTT_TOPIC  = "sensores/camera"; 

// ─── Parâmetros ───────────────────────────────────────────────────────────────
static const unsigned long CHECK_INTERVAL_MS  = 2000;
static const unsigned long MIN_PUBLISH_GAP_MS = 10000;
static const uint8_t  PIXEL_DIFF_TH     = 20;
static const uint8_t  MOTION_PERCENT_TH =  5;
static const uint8_t  DIFF_STEP         = 16;   // amostra 1:16 pixels no diff
static const uint8_t  JPEG_QUALITY      = 85;

// ─── LED Flash ────────────────────────────────────────────────────────────────
#define LED_PIN  4
#define LED_ON()  digitalWrite(LED_PIN, HIGH)
#define LED_OFF() digitalWrite(LED_PIN, LOW)

// ─── Certificados TLS ────────────────────────────────────────────────────────
static const char CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
#sua CA aqui
-----END CERTIFICATE-----
)EOF";

static const char CLIENT_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
#sua cert aqui
-----END CERTIFICATE-----
)EOF";

static const char CLIENT_KEY[] PROGMEM = R"EOF(
-----BEGIN PRIVATE KEY-----
#sua chave privada aqui
-----END PRIVATE KEY-----
)EOF";

// ─── Pinos AI Thinker ─────────────────────────────────────────────────────────
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

// ─── Globais ──────────────────────────────────────────────────────────────────
WiFiClientSecure espClient;
PubSubClient     mqttClient(espClient);
static uint8_t*  prevFrame    = nullptr;
static size_t    prevFrameLen = 0;   // definido após init da câmera

void reconnectMQTT();

// ─────────────────────────────────────────────────────────────────────────────
bool initCamera() {
  camera_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));

  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0  = Y2_GPIO_NUM;  cfg.pin_d1  = Y3_GPIO_NUM;
  cfg.pin_d2  = Y4_GPIO_NUM;  cfg.pin_d3  = Y5_GPIO_NUM;
  cfg.pin_d4  = Y6_GPIO_NUM;  cfg.pin_d5  = Y7_GPIO_NUM;
  cfg.pin_d6  = Y8_GPIO_NUM;  cfg.pin_d7  = Y9_GPIO_NUM;
  cfg.pin_xclk     = XCLK_GPIO_NUM;
  cfg.pin_pclk     = PCLK_GPIO_NUM;
  cfg.pin_vsync    = VSYNC_GPIO_NUM;
  cfg.pin_href     = HREF_GPIO_NUM;
  cfg.pin_sscb_sda = SIOD_GPIO_NUM;
  cfg.pin_sscb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn     = PWDN_GPIO_NUM;
  cfg.pin_reset    = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;

  // VGA GRAYSCALE fixo — NUNCA muda de resolução em runtime
  cfg.pixel_format = PIXFORMAT_GRAYSCALE;
  cfg.frame_size   = FRAMESIZE_VGA;       // 640×480
  cfg.jpeg_quality = 12;
  cfg.fb_count     = psramFound() ? 2 : 1;

  if (esp_camera_init(&cfg) != ESP_OK) {
    Serial.println("[CAM] Falha init!");
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s,  1);
    s->set_contrast(s,    1);
    s->set_ae_level(s,    0);
    s->set_gainceiling(s, (gainceiling_t)1);
    s->set_whitebal(s,    1);
    s->set_awb_gain(s,    1);
    s->set_aec2(s,        1);
    s->set_aec_value(s, 300);
  }

  // Captura um frame real para descobrir o tamanho correto do buffer
  camera_fb_t* probe = esp_camera_fb_get();
  if (!probe) {
    Serial.println("[CAM] Frame de probe nulo!");
    return false;
  }
  prevFrameLen = probe->len;
  esp_camera_fb_return(probe);

  Serial.printf("[CAM] Frame real: %u bytes | PSRAM: %s\n",
                prevFrameLen, psramFound() ? "sim" : "nao");

  prevFrame = psramFound()
    ? (uint8_t*)ps_malloc(prevFrameLen)
    : (uint8_t*)malloc(prevFrameLen);

  if (!prevFrame) {
    Serial.println("[CAM] ERRO: sem RAM para diff buffer!");
    return false;
  }
  memset(prevFrame, 128, prevFrameLen);

  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
int detectMotion() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return -1;

  // Atualiza tamanho real se por algum motivo mudou
  if (fb->len != prevFrameLen) {
    Serial.printf("[MOTION] Tamanho mudou: %u → %u, atualizando buffer\n",
                  prevFrameLen, fb->len);
    uint8_t* tmp = psramFound()
      ? (uint8_t*)ps_realloc(prevFrame, fb->len)
      : (uint8_t*)realloc(prevFrame, fb->len);
    if (tmp) {
      prevFrame    = tmp;
      prevFrameLen = fb->len;
      memset(prevFrame, 128, prevFrameLen);
    }
    esp_camera_fb_return(fb);
    return -1;   // pula este ciclo
  }

  int changed = 0, total = 0;
  for (size_t i = 0; i < fb->len; i += DIFF_STEP) {
    if (abs((int)fb->buf[i] - (int)prevFrame[i]) > PIXEL_DIFF_TH) changed++;
    total++;
  }

  memcpy(prevFrame, fb->buf, fb->len);
  esp_camera_fb_return(fb);

  return total > 0 ? (changed * 100) / total : 0;
}

// ─────────────────────────────────────────────────────────────────────────────
bool captureAndPublish() {
  LED_ON();
  delay(200);

  // Descarta frames de aquecimento (AEC convergindo)
  for (int i = 0; i < 4; i++) {
    camera_fb_t* tmp = esp_camera_fb_get();
    if (tmp) esp_camera_fb_return(tmp);
    delay(50);
  }

  camera_fb_t* fb = esp_camera_fb_get();
  LED_OFF();

  if (!fb || fb->len == 0) {
    Serial.println("[CAP] Frame nulo ou vazio!");
    if (fb) esp_camera_fb_return(fb);
    return false;
  }

  Serial.printf("[CAP] Frame: %dx%d | %u bytes\n", fb->width, fb->height, fb->len);

  // ← VALIDAÇÃO CRÍTICA: garante que o tamanho bate com as dimensões
  size_t expectedLen = (size_t)fb->width * fb->height;
  if (fb->len != expectedLen) {
    Serial.printf("[CAP] ERRO: esperado %u bytes para %dx%d, recebi %u — abortando\n",
                  expectedLen, fb->width, fb->height, fb->len);
    esp_camera_fb_return(fb);
    return false;
  }

  uint8_t* jpgBuf = nullptr;
  size_t   jpgLen = 0;
  bool converted  = frame2jpg(fb, JPEG_QUALITY, &jpgBuf, &jpgLen);
  esp_camera_fb_return(fb);

  memset(prevFrame, 128, prevFrameLen);   // reseta diff

  if (!converted || !jpgBuf || jpgLen == 0) {
    Serial.println("[CAP] frame2jpg falhou!");
    if (jpgBuf) free(jpgBuf);
    return false;
  }

  Serial.printf("[CAP] JPEG: %u bytes (qualidade %d)\n", jpgLen, JPEG_QUALITY);

  // Sanidade: JPEG válido começa com FF D8
  if (jpgLen < 4 || jpgBuf[0] != 0xFF || jpgBuf[1] != 0xD8) {
    Serial.println("[CAP] JPEG inválido (header errado) — descartando");
    free(jpgBuf);
    return false;
  }

  if (!mqttClient.connected()) reconnectMQTT();

  bool ok = mqttClient.beginPublish(MQTT_TOPIC, jpgLen, false);
  if (!ok) {
    Serial.println("[MQTT] beginPublish falhou!");
    free(jpgBuf);
    return false;
  }

  const size_t CHUNK = 4096;
  size_t sent = 0;
  while (sent < jpgLen) {
    size_t n = min(CHUNK, jpgLen - sent);
    size_t written = mqttClient.write(jpgBuf + sent, n);
    if (written != n) {
      Serial.printf("[MQTT] write parcial: %u/%u bytes\n", written, n);
      free(jpgBuf);
      return false;
    }
    sent += n;
    mqttClient.loop();   // mantém keepalive durante chunks
  }

  ok = mqttClient.endPublish();
  Serial.printf("[MQTT] endPublish: %s | %u bytes enviados\n",
                ok ? "OK" : "FALHOU", sent);

  free(jpgBuf);
  return ok;
}

// ─── NTP ─────────────────────────────────────────────────────────────────────
void syncTime() {
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("[NTP] Sincronizando");
  time_t now = time(nullptr);
  while (now < 1000000000UL) { delay(500); Serial.print("."); now = time(nullptr); }
  struct tm t;
  localtime_r(&now, &t);
  Serial.printf("\n[NTP] %02d/%02d/%04d %02d:%02d:%02d\n",
    t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour, t.tm_min, t.tm_sec);
}

// ─── MQTT reconexão ──────────────────────────────────────────────────────────
void reconnectMQTT() {
  int attempts = 0;
  while (!mqttClient.connected()) {
    Serial.printf("[MQTT] Conectando (tentativa %d)... ", ++attempts);
    if (mqttClient.connect(MQTT_CLIENT)) {
      Serial.println("OK!");
    } else {
      Serial.printf("falhou rc=%d. Aguardando 5s\n", mqttClient.state());
      for (int i = 0; i < 10; i++) { delay(500); mqttClient.loop(); }
    }
    if (attempts >= 5) {
      Serial.println("[MQTT] Muitas falhas — reiniciando");
      ESP.restart();
    }
  }
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  LED_OFF();

  if (!initCamera()) {
    delay(3000);
    ESP.restart();
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WIFI] Conectando");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\n[WIFI] IP: %s  RSSI: %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());

  syncTime();

  espClient.setCACert(CA_CERT);
  espClient.setCertificate(CLIENT_CERT);
  espClient.setPrivateKey(CLIENT_KEY);
  espClient.setHandshakeTimeout(30);

  mqttClient.setBufferSize(4096);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  reconnectMQTT();
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();

  static unsigned long lastCheck   = 0;
  static unsigned long lastPublish = 0;

  if (millis() - lastCheck < CHECK_INTERVAL_MS) return;
  lastCheck = millis();

  int pct = detectMotion();
  if (pct < 0) return;

  Serial.printf("[MOTION] %d%% pixels alterados (limiar: %d%%)\n", pct, MOTION_PERCENT_TH);

  if (pct < (int)MOTION_PERCENT_TH) return;

  unsigned long elapsed = millis() - lastPublish;
  if (elapsed < MIN_PUBLISH_GAP_MS) {
    Serial.printf("[MOTION] Cooldown ativo (%lu s)\n", (MIN_PUBLISH_GAP_MS - elapsed) / 1000);
    return;
  }

  Serial.printf("[MOTION] *** Movimento! %d%% — capturando ***\n", pct);

  bool ok = captureAndPublish();
  Serial.println(ok ? "[MQTT] Foto enviada!" : "[MQTT] ERRO ao publicar");

  if (ok) lastPublish = millis();
}