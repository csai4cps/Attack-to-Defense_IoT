import sys
import os
from collections import defaultdict

import pandas as pd
from scapy.all import rdpcap, IP, TCP, UDP, Raw
from tqdm import tqdm

if len(sys.argv) < 4:
    print("Usage: python extract.py <file.pcap> <label> <output.csv>")
    sys.exit(1)

PCAP_FILE   = sys.argv[1]
LABEL       = sys.argv[2]
OUTPUT_FILE = sys.argv[3]

os.makedirs(os.path.dirname(OUTPUT_FILE) or ".", exist_ok=True)

print(f"Reading {PCAP_FILE}...")
packets = rdpcap(PCAP_FILE)
print(f"{len(packets)} packets loaded")

flows = defaultdict(list)

for pkt in tqdm(packets, desc="Indexing flows"):
    if IP not in pkt:
        continue
    proto = pkt[IP].proto
    sp = dp = 0
    flags = ""

    if TCP in pkt:
        sp    = pkt[TCP].sport
        dp    = pkt[TCP].dport
        flags = str(pkt[TCP].flags)
    elif UDP in pkt:
        sp = pkt[UDP].sport
        dp = pkt[UDP].dport

    key = (pkt[IP].src, pkt[IP].dst, proto, sp, dp)
    flows[key].append({
        "time":    float(pkt.time),
        "len":     len(pkt),
        "ttl":     pkt[IP].ttl,
        "flags":   flags,
        "raw_len": len(pkt[Raw].load) if Raw in pkt else 0,
    })

print(f"{len(flows)} flows identified")

rows = []

for (src_ip, dst_ip, proto, src_port, dst_port), pkts in tqdm(flows.items(), desc="Extracting features"):
    if len(pkts) < 1:
        continue

    times   = [p["time"] for p in pkts]
    lengths = [p["len"]  for p in pkts]
    ttls    = [p["ttl"]  for p in pkts]
    iat     = [times[i] - times[i-1] for i in range(1, len(times))]

    all_flags = " ".join(set(p["flags"] for p in pkts if p["flags"]))
    proto_name = {6: "TCP", 17: "UDP"}.get(proto, str(proto))

    rows.append({
        "src_ip":         src_ip,
        "dst_ip":         dst_ip,
        "protocol":       proto_name,
        "src_port":       src_port,
        "dst_port":       dst_port,
        "pkt_count":      len(pkts),
        "total_bytes":    sum(lengths),
        "mean_pkt_len":   round(sum(lengths) / len(lengths), 2),
        "max_pkt_len":    max(lengths),
        "min_pkt_len":    min(lengths),
        "duration_s":     round(max(times) - min(times), 4),
        "mean_iat":       round(sum(iat) / len(iat), 4) if iat else 0,
        "max_iat":        round(max(iat), 4) if iat else 0,
        "mean_ttl":       round(sum(ttls) / len(ttls), 2),
        "has_syn":        1 if "S" in all_flags else 0,
        "has_fin":        1 if "F" in all_flags else 0,
        "has_rst":        1 if "R" in all_flags else 0,
        "has_psh":        1 if "P" in all_flags else 0,
        "has_ack":        1 if "A" in all_flags else 0,
        "raw_pkt_count":  sum(1 for p in pkts if p["raw_len"] > 0),
        "total_raw_bytes": sum(p["raw_len"] for p in pkts),
        "label":          LABEL,
    })

df = pd.DataFrame(rows)
df.to_csv(OUTPUT_FILE, index=False)
print(f"\nSaved {len(df)} flows to {OUTPUT_FILE}")
print(df.describe())
