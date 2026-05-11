#pragma once
#include <pgmspace.h>

// ─── Wi-Fi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID     "SecurityWifi"
#define WIFI_PASSWORD "123456789"

// ─── MQTT / Broker ────────────────────────────────────────────────────────────
#define MQTT_HOST   "ec2-23-20-167-220.compute-1.amazonaws.com"
#define MQTT_PORT   8883
#define MQTT_CLIENT "cardputer_01"

// ─── Tópicos  (a label vem do tópico no broker, NÃO do payload) ──────────────
#define TOPIC_NORMAL "iot/card"  
#define TOPIC_ATTACK "iot/card"  // mesmo tópico para flood, mas o payload é idêntico ao normal — só o volume/frequência denuncia o ataque

// ─── Device ───────────────────────────────────────────────────────────────────
#define DEVICE_ID   "cardputer_01"    // nome do seu dispositivo
#define DEVICE_SRC  "200.123.22.65"   // IP de origem simulado nos payloads de ataque

// ─── Web UI ───────────────────────────────────────────────────────────────────
#define WEBUI_PORT  80

static const char BROKER_CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
#sua CA aqui
-----END CERTIFICATE-----
)EOF";

// ─── 2. Certificado do cliente (cardputer.crt) ────────────────────────────────
static const char CLIENT_CERT[] PROGMEM = R"KEY(
-----BEGIN CERTIFICATE-----
#sua cert aqui
-----END CERTIFICATE-----
)KEY";

// ─── 3. Chave privada do cliente (cardputer.key) ──────────────────────────────
static const char CLIENT_KEY[] PROGMEM = R"KEY(
-----BEGIN PRIVATE KEY-----
#sua chave privada aqui
-----END PRIVATE KEY-----
)KEY";