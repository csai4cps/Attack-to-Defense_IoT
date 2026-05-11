
# A2D-IoT — Ambiente Experimental de Segurança para IoT

> Plataforma física e reprodutível para geração de datasets rotulados e detecção de ataques em ambientes IoT com Machine Learning.

---

## Visão Geral

O projeto combina hardware real (ESP32), infraestrutura em nuvem (AWS) e modelos de Machine Learning para capturar, rotular e classificar tráfego IoT legítimo e malicioso. A arquitetura é híbrida: processamento local nos dispositivos ESP32 e análise centralizada na nuvem.

---

## Arquitetura

![Diagrama da Arquitetura A2D-IoT](img/Diagrama-A2D_IoT.jpeg)

```
[M5Stack Cardputer] ──┐
                       ├──> [ESP32-C5 Router STA+AP] ──> [AWS EC2 Mosquitto MQTT/TLS]
[ESP32-CAM AI Thinker]─┘                                         │
                                                          ┌───────┴────────┐
                                                     [Amazon S3]   [Feature Extractor]
                                                                          │
                                                                    [ML Models]
```

- **ESP32-C5** — roteador local (modo STA+AP), captura pacotes em PCAP  
- **M5Stack Cardputer** — envia telemetria MQTT e gera ataques de flood  
- **ESP32-CAM** — detecção de movimento por diferença de quadros, publica imagens via MQTT  
- **AWS EC2** — broker Mosquitto com TLS/PKI própria, extração de features e encaminhamento para S3  

---

## Estrutura do Repositório

```
├── ML/
│   └── Training-Model.py          # Treinamento dos modelos de ML
├── data_collector/
│   ├── pcap-to-feature/
│   │   └── pcap-extract.py        # Extração de features de arquivos PCAP
│   └── topic-to-feature/
│       └── Topic-collector.py     # Extração de features dos tópicos MQTT
├── firmware/
│   ├── Cardputer-A2D/
│   │   ├── Cardputer-A2D.ino      # Firmware do M5Stack Cardputer
│   │   └── secrets.h              # Credenciais (não versionar)
│   └── ESP32CAM-A2D/
│       └── src/
│           ├── main.cpp           # Firmware da ESP32-CAM
│           └── platformio.ini
├── img/
│   └── Diagrama-A2D_IoT.jpeg
└── README.md
```

---

## Ataques Avaliados

| Ataque | Ferramenta / Vetor |
|---|---|
| C2 Beaconing | Sliver C2 |
| Flood MQTT | Cardputer (tópicos MQTT) |
| DoS | hping3 |
| Scan de rede | Nmap |
| Brute Force | Painel web do Cardputer |

---

## Modelos de Machine Learning

Random Forest · Decision Tree · KNN · SVM · MLP

Avaliados por acurácia, F1-score e custo computacional em cenários IoT reais.

---

## Dataset

O dataset é multimodal e gerado automaticamente pelo pipeline:

- **Tráfego de rede** → features extraídas de arquivos PCAP  
- **Telemetria MQTT** → features extraídas dos tópicos publicados  
- **Eventos visuais** → imagens da ESP32-CAM armazenadas no S3  

---

## Tecnologias

`ESP32` `ESP32-C5` `ESP32-CAM` `M5Stack Cardputer` `MQTT/TLS` `Mosquitto` `AWS EC2` `Amazon S3` `Python` `PlatformIO` `Scikit-learn`

---

## Contribuições Científicas

1. Ambiente IoT físico e reprodutível para pesquisa em cibersegurança  
2. Dataset rotulado multimodal (rede + MQTT + visão)  
3. Pipeline automatizado de coleta e extração de features  
4. Avaliação de ataques reais em dispositivos embarcados  
5. Comparação de algoritmos supervisionados para detecção de intrusão  
6. Integração edge computing + cloud computing para segurança IoT  
7. Base experimental extensível para Federated Learning e CPS Security  

---

## Aviso

O arquivo `secrets.h` contém credenciais sensíveis. **Não o versione.** Adicione-o ao `.gitignore`.
=======
# README — Attack-to-Defense IoT

An end-to-end Cybersecurity IoT Testbed for Attack Simulation, Telemetry Collection, Dataset Generation, and AI-based Intrusion Detection Research in Cyber-Physical Systems (CPS).

<p align="center">
  <img src="docs/images/architecture.png" width="800">
</p>


## Overview

The **Attack-to-Defense IoT** project provides a realistic and reproducible cybersecurity environment focused on:

- IoT and MQTT attack simulation;
- Cyber-Physical Systems (CPS) security experimentation;
- Network telemetry collection;
- Labeled dataset generation;
- AI/ML-based intrusion detection research;
- Federated Learning and adaptive defense experiments.

The platform integrates offensive and defensive cybersecurity workflows in a single environment, enabling researchers, students, and practitioners to reproduce realistic attack scenarios against IoT infrastructures and evaluate detection and mitigation techniques.

The project was designed as part of ongoing research related to:

- IoT Security;
- Operational Technology (OT) Security;
- Cyber-Physical Systems (CPS);
- MQTT Threat Detection;
- Federated Learning Security;
- AI for Cybersecurity.


# Architecture

The environment combines physical IoT devices, attack infrastructure, telemetry collection pipelines, and Machine Learning analysis.

```text
                +----------------------+
                |   Attacker Machine   |
                | Kali Linux / Sliver  |
                +----------+-----------+
                           |
                           |
                    Attack Traffic
                           |
                           v
+----------------------------------------------------+
|                IoT Network Environment             |
|                                                    |
|  +-------------+     +--------------------------+ |
|  | Cardputer   | --> | MQTT Broker (Mosquitto) | |
|  +-------------+     +--------------------------+ |
|         |                        |                |
|         | MQTT Telemetry         |                |
|         v                        v                |
|  +---------------------------------------------+ |
|  | Telemetry & Packet Collection Pipeline      | |
|  | tshark / tcpdump / PCAP / MQTT extraction   | |
|  +---------------------------------------------+ |
+----------------------------------------------------+
                           |
                           v
                +----------------------+
                | Dataset Generation   |
                | CSV / PCAP / Labels  |
                +----------------------+
                           |
                           v
                +----------------------+
                | AI / ML Detection    |
                | Weka / Orange / FL   |
                +----------------------+
````


# Main Features

## Offensive Security Scenarios

The environment supports realistic attack execution against MQTT-enabled IoT infrastructures.

Implemented attack scenarios include:

* MQTT flooding attacks;
* MQTT malformed packet injection;
* Sliver Command and Control (C2) beaconing traffic;
* Denial-of-Service (DoS) attacks using `hping3`;
* Network reconnaissance using `Nmap`;
* Brute-force attacks against embedded web services;
* Adversarial traffic generation;
* Packet replay attacks.


## Defensive Security Capabilities

The project also supports defensive cybersecurity experimentation, including:

* Network traffic inspection;
* Intrusion Detection Systems (IDS);
* Machine Learning classification;
* Feature selection;
* Anomaly detection;
* Behavioral analysis;
* Threat telemetry extraction;
* Federated Learning experiments;
* Dataset enrichment pipelines.


# Technologies Used

## Infrastructure

* Docker
* Docker Compose
* Linux
* Mosquitto MQTT
* Kali Linux
* Raspberry Pi
* Cardputer
* ESP32
* Ubuntu Server

## Offensive Security

* Sliver C2
* hping3
* Nmap
* Hydra
* Metasploit

## Packet Collection

* tshark
* tcpdump
* Wireshark
* CICFlowMeter

## AI / Data Science

* Python
* Pandas
* Scikit-learn
* TensorFlow
* Orange Data Mining
* Weka
* Federated Learning Frameworks


# Project Structure

```text
Attack-to-Defense_IoT/
│
├── datasets/
│   ├── raw/
│   ├── processed/
│   └── labeled/
│
├── pcap/
│
├── telemetry/
│
├── scripts/
│   ├── attacks/
│   ├── collection/
│   ├── preprocessing/
│   └── ml/
│
├── docker/
│
├── docs/
│   ├── images/
│   └── papers/
│
├── notebooks/
│
├── models/
│
├── results/
│
└── README.md
```



# Installation

## Clone Repository

```bash
git clone https://github.com/csai4cps/Attack-to-Defense_IoT.git

cd Attack-to-Defense_IoT
```


# Requirements

## System Requirements

* Ubuntu 22.04+ (recommended)
* Docker
* Python 3.10+
* Wireshark/tshark
* 8 GB RAM minimum

# Docker Environment

Start the environment:

```bash
docker compose up -d
```

Check containers:

```bash
docker ps
```

Stop environment:

```bash
docker compose down
```


# Running Attack Scenarios

## MQTT Flood Attack

```bash
python3 scripts/attacks/mqtt_flood.py
```

## DoS with hping3

```bash
sudo hping3 -S --flood -V <TARGET_IP>
```

## Nmap Scan

```bash
nmap -sV -A <TARGET_IP>
```

## Sliver C2 Beacon

Start Sliver server and generate implant payloads.



# Packet Collection

## tshark

```bash
sudo tshark -i eth0 -w attack_capture.pcap
```

## tcpdump

```bash
sudo tcpdump -i eth0 -w attack_capture.pcap
```


# Dataset Generation

The collected traffic can be converted into structured datasets for AI training.

Example pipeline:

```text
PCAP -> CICFlowMeter -> CSV -> Feature Engineering -> ML Dataset
```

Generated datasets may include:

* Benign MQTT traffic;
* Flood attacks;
* Beaconing traffic;
* Port scanning;
* Brute-force authentication attempts;
* Mixed attack scenarios.


# Machine Learning Pipeline

Supported workflows:

* Feature Selection;
* Random Forest;
* XGBoost;
* Isolation Forest;
* Autoencoders;
* Federated Learning;
* Adversarial ML evaluation.

Example training:

```bash
python3 scripts/ml/train_random_forest.py
```


# Research Applications

This environment can be used for:

* IoT intrusion detection;
* CPS cybersecurity research;
* MQTT threat analysis;
* AI-driven defense evaluation;
* Cyber range exercises;
* Security education;
* Dataset generation for academia;
* Federated Learning security studies;
* Data poisoning defense evaluation.

# Publications

This project supports ongoing academic research related to:

* CSAI-4-CPS;
* MQTT threat detection;
* IoT intrusion detection;
* CPS resilience;
* Federated Learning security.


# Future Work

Planned improvements include:

* Real-time streaming inference;
* Distributed Federated Learning nodes;
* Additional IoT protocols;
* Digital twin integration;
* OT protocol support;
* Edge AI deployment;
* SIEM integration;
* SOAR orchestration;
* Threat Intelligence enrichment.


# Contributing

Contributions are welcome.

To contribute:

1. Fork the repository;
2. Create a feature branch;
3. Commit your changes;
4. Open a Pull Request.


# License

This project is released under the MIT License.



# Acknowledgements

This project was developed with support from:

* SENAI São Caetano do Sul
* UNICAMP
* CSAI-4-CPS Research Group

```
```
>>>>>>> 64657b946743a016109e1be1f9c6fccd6573a692
