iot-mqtt-lab/
├── docker-compose.yml
├── mosquitto/
│   └── mosquitto.conf
├── scripts/
│   ├── gen-certs.sh     geração automática de CA + certs de servidor e 3 clientes
│   ├── collector.py     assina tópicos MQTT e salva CSV rotulado por IAT
│   ├── extract.py       extrai features de fluxo de arquivos .pcap
│   └── train.py         treina 5 modelos ML e salva resultados em results/
├── certs/               gerado automaticamente no primeiro `docker compose up`
│   ├── ca.crt / ca.key
│   ├── server.crt / server.key
│   └── clients/
│       ├── espcam.crt / espcam.key
│       ├── cardputer.crt / cardputer.key
│       └── kali.crt / kali.key
├── dataset/             CSVs gerados pelo collector e pelo extract.py
└── results/             gráficos e rankings gerados pelo train.py

────────────────────────────────────────────────────────
INICIAR INFRAESTRUTURA
────────────────────────────────────────────────────────

docker compose up -d

  Na primeira execução o serviço cert-gen gera todos os certificados
  automaticamente e depois o mosquitto sobe com TLS 1.2 + mTLS.

────────────────────────────────────────────────────────
COPIAR CERTIFICADOS PARA OS DISPOSITIVOS
────────────────────────────────────────────────────────

Copiar para o ESP32-CAM (via esptool ou embeddado no firmware):
  certs/ca.crt
  certs/clients/espcam.crt
  certs/clients/espcam.key

Copiar para o Cardputer (mesmo processo):
  certs/ca.crt
  certs/clients/cardputer.crt
  certs/clients/cardputer.key

Copiar para o Kali (para mosquitto_pub/sub e hydra tests):
  certs/ca.crt
  certs/clients/kali.crt
  certs/clients/kali.key

────────────────────────────────────────────────────────
EXTRAIR FEATURES DE PCAP
────────────────────────────────────────────────────────

pip install scapy pandas tqdm

python scripts/extract.py dataset/bruteforce.pcap  brute   dataset/pcap_brute.csv
python scripts/extract.py dataset/scan.pcap         scan    dataset/pcap_scan.csv
python scripts/extract.py dataset/trafegolegitmo.pcap benign dataset/pcap_benign.csv
python scripts/extract.py dataset/c2_beacon.pcap    c2      dataset/pcap_c2.csv
python scripts/extract.py dataset/syn_flood.pcap    flood   dataset/pcap_flood.csv

────────────────────────────────────────────────────────
TREINAR OS MODELOS
────────────────────────────────────────────────────────

docker compose --profile train run --rm trainer

  Lê todos os CSVs em dataset/, detecta automaticamente se são MQTT ou PCAP,
  treina Decision Tree, Random Forest, KNN, SVM e MLP,
  salva matrizes de confusão, feature importance e ranking em results/.

────────────────────────────────────────────────────────
PARAR
────────────────────────────────────────────────────────

docker compose down
