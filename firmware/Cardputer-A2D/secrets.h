#pragma once
#include <pgmspace.h>

// ─── Wi-Fi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID     "SecurityWifi" //     SEU SSID
#define WIFI_PASSWORD "123456789" //   SUA SENHA

// ─── MQTT / Broker ────────────────────────────────────────────────────────────
#define MQTT_HOST   "IP" // BROKER
#define MQTT_PORT   8883 // SUA PORTA 883 para Tls e 1883 para não-TLS
#define MQTT_CLIENT "cardputer_01" // SEU CLIENT ID

// ─── Tópicos  ──────────────
#define TOPIC_NORMAL "iot/card"   
#define TOPIC_ATTACK "iot/card"   //define o topico de ataque
// ─── Device ───────────────────────────────────────────────────────────────────
#define DEVICE_ID   "cardputer_01"
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
#   sua chave privada aqui
-----END PRIVATE KEY-----
)KEY";