import os
import csv
import json
import time
import logging
from datetime import datetime, timezone
import paho.mqtt.client as mqtt

MQTT_HOST      = os.getenv("MQTT_HOST", "mosquitto")
MQTT_PORT      = int(os.getenv("MQTT_PORT", "8883"))
TOPIC_CARD     = os.getenv("TOPIC_CARD", "iot/card")
TOPIC_CAMERA   = os.getenv("TOPIC_CAMERA", "sensores/camera")
IAT_THRESHOLD  = float(os.getenv("IAT_THRESHOLD", "0.5"))

CA_CERT        = "/caminho/ca.crt"        # CA do broker
CLIENT_CERT    = "/caminho/seu_cert.crt"  # cert do cliente
CLIENT_KEY     = "/caminho/sua_chave.key"   # chave privada do cliente

DATASET_DIR    = "dataset"
os.makedirs(DATASET_DIR, exist_ok=True)
CSV_FILE = os.path.join(DATASET_DIR, f"mqtt_{datetime.now().strftime('%Y%m%d_%H%M')}.csv")

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
log = logging.getLogger(__name__)

FIELDNAMES = [
    "collected_at", "topic", "label", "iat_ms",
    "device", "fw_ts", "temperature", "humidity", "pressure", "rssi", "seq",
    "image_bytes",
]

_f   = open(CSV_FILE, "w", newline="", buffering=1)
_csv = csv.DictWriter(_f, fieldnames=FIELDNAMES, extrasaction="ignore")
_csv.writeheader()

count = count_benign = count_flood = count_camera = 0
_last_ts: dict[str, float] = {}


def save(row: dict):
    global count
    full = {f: "" for f in FIELDNAMES}
    full.update(row)
    _csv.writerow(full)
    count += 1
    if count % 50 == 0:
        log.info("saved=%d  benign=%d  flood=%d  camera=%d",
                 count, count_benign, count_flood, count_camera)


def get_iat(topic: str) -> float:
    now = time.monotonic()
    iat = now - _last_ts[topic] if topic in _last_ts else 9999.0
    _last_ts[topic] = now
    return iat


def handle_card(payload: bytes, topic: str):
    global count_benign, count_flood
    iat   = get_iat(topic)
    label = "flood" if iat < IAT_THRESHOLD else "benign"
    ts    = datetime.now(timezone.utc).isoformat()

    try:
        doc = json.loads(payload)
    except Exception:
        save({"collected_at": ts, "topic": topic, "label": label,
              "iat_ms": round(iat * 1000, 1)})
        return

    save({
        "collected_at": ts,
        "topic":        topic,
        "label":        label,
        "iat_ms":       round(iat * 1000, 1),
        "device":       doc.get("d",    ""),
        "fw_ts":        doc.get("ts",   ""),
        "temperature":  doc.get("t",    ""),
        "humidity":     doc.get("h",    ""),
        "pressure":     doc.get("p",    ""),
        "rssi":         doc.get("rssi", ""),
        "seq":          doc.get("seq",  ""),
    })

    if label == "flood":
        count_flood += 1
    else:
        count_benign += 1


def handle_camera(payload: bytes, topic: str):
    global count_camera
    count_camera += 1
    save({
        "collected_at": datetime.now(timezone.utc).isoformat(),
        "topic":        topic,
        "label":        "benign",
        "iat_ms":       round(get_iat(topic) * 1000, 1),
        "image_bytes":  len(payload),
    })


HANDLERS = {TOPIC_CARD: handle_card, TOPIC_CAMERA: handle_camera}


def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        log.info("Connected to broker")
        for t in HANDLERS:
            client.subscribe(t, qos=1)
            log.info("Subscribed: %s", t)
    else:
        log.error("Connection failed rc=%d", rc)


def on_message(client, userdata, msg):
    h = HANDLERS.get(msg.topic)
    if h:
        h(msg.payload, msg.topic)


def on_disconnect(client, userdata, rc, properties=None):
    _f.flush()
    if rc != 0:
        log.warning("Unexpected disconnect rc=%d", rc)


def main():
    log.info("Starting collector — IAT threshold=%.0fms", IAT_THRESHOLD * 1000)
    log.info("Output: %s", CSV_FILE)

    client = mqtt.Client(client_id="collector",
                         callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    client.tls_set(ca_certs=CA_CERT, certfile=CLIENT_CERT, keyfile=CLIENT_KEY)
    client.on_connect    = on_connect
    client.on_message    = on_message
    client.on_disconnect = on_disconnect
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        log.info("Stopped. total=%d  benign=%d  flood=%d  camera=%d",
                 count, count_benign, count_flood, count_camera)
        _f.close()


if __name__ == "__main__":
    main()
