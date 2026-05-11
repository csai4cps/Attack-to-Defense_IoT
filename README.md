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