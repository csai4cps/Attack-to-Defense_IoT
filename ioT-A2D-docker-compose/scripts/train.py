import os
import glob
import warnings
warnings.filterwarnings("ignore")

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

from sklearn.model_selection import train_test_split, cross_val_score
from sklearn.preprocessing   import LabelEncoder, StandardScaler
from sklearn.pipeline        import Pipeline
from sklearn.metrics         import classification_report, confusion_matrix

from sklearn.tree            import DecisionTreeClassifier
from sklearn.ensemble        import RandomForestClassifier
from sklearn.neighbors       import KNeighborsClassifier
from sklearn.svm             import SVC
from sklearn.neural_network  import MLPClassifier

DATASET_DIR = "dataset"
OUTPUT_DIR  = "results"
os.makedirs(OUTPUT_DIR, exist_ok=True)

MQTT_FEATURES = ["iat_ms", "temperature", "humidity", "pressure", "rssi"]

PCAP_FEATURES = [
    "pkt_count", "total_bytes", "mean_pkt_len", "max_pkt_len", "min_pkt_len",
    "duration_s", "mean_iat", "max_iat", "mean_ttl",
    "has_syn", "has_fin", "has_rst", "has_psh", "has_ack",
    "raw_pkt_count", "total_raw_bytes", "dst_port", "src_port",
]

MODELS = {
    "Decision Tree": DecisionTreeClassifier(max_depth=12, random_state=42),
    "Random Forest": RandomForestClassifier(n_estimators=100, random_state=42, n_jobs=-1),
    "KNN":           KNeighborsClassifier(n_neighbors=5),
    "SVM":           Pipeline([("sc", StandardScaler()), ("svm", SVC(kernel="rbf", random_state=42))]),
    "MLP":           Pipeline([("sc", StandardScaler()), ("mlp", MLPClassifier(hidden_layer_sizes=(64, 32), max_iter=300, random_state=42))]),
}


def load_csvs():
    mqtt_dfs, pcap_dfs = [], []
    files = sorted(glob.glob(os.path.join(DATASET_DIR, "*.csv")))
    print(f"Found {len(files)} CSV file(s)")

    for f in files:
        df = pd.read_csv(f)
        if "label" in df.columns:
            df["label"] = df["label"].str.strip().replace(
                {"benig": "benign", "legitimate": "benign", "normal": "benign"}
            )
        name = os.path.basename(f)
        if "iat_ms" in df.columns:
            mqtt_dfs.append(df)
            print(f"  [MQTT] {name}: {len(df)} rows  {df['label'].value_counts().to_dict()}")
        elif "pkt_count" in df.columns:
            pcap_dfs.append(df)
            print(f"  [PCAP] {name}: {len(df)} rows  {df['label'].value_counts().to_dict()}")
        else:
            print(f"  [?]   {name}: unknown schema — skipped")

    return mqtt_dfs, pcap_dfs


def prepare_mqtt(dfs):
    df = pd.concat(dfs, ignore_index=True)
    for col in MQTT_FEATURES:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
    if "image_bytes" in df.columns:
        df = df[df["image_bytes"].isna() | (df["image_bytes"] == "")]
    return df.dropna(subset=["iat_ms", "label"])


def run(df, features, title):
    available = [c for c in features if c in df.columns]
    df_clean  = df[available + ["label"]].dropna()

    if len(df_clean) < 50:
        print(f"\n[!] {title}: insufficient data ({len(df_clean)} rows) — skipping")
        return

    vc    = df_clean["label"].value_counts()
    ratio = vc.max() / max(vc.min(), 1)
    slug  = title.lower().replace(" ", "_")

    print(f"\n{'='*60}")
    print(f"{title}  |  rows={len(df_clean)}  |  features={available}")
    print(vc.to_string())
    if ratio > 10:
        print(f"[!] Imbalance ratio {ratio:.0f}x")
    print("="*60)

    fig, ax = plt.subplots(figsize=(8, 4))
    vc.plot(kind="bar", ax=ax, color="steelblue")
    ax.set_title(f"Distribution — {title}")
    ax.set_ylabel("Count")
    ax.tick_params(axis="x", rotation=30)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, f"dist_{slug}.png"), dpi=150)
    plt.close()

    le = LabelEncoder()
    y  = le.fit_transform(df_clean["label"])
    X  = df_clean[available].values

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y
    )

    results = {}

    for name, model in MODELS.items():
        model.fit(X_train, y_train)
        y_pred = model.predict(X_test)
        rep    = classification_report(y_test, y_pred, target_names=le.classes_,
                                       output_dict=True, zero_division=0)
        acc = rep["accuracy"]
        f1  = rep["macro avg"]["f1-score"]
        results[name] = {"accuracy": round(acc, 4), "f1_macro": round(f1, 4)}

        print(f"\n[{name}]  acc={acc:.4f}  f1={f1:.4f}")
        print(classification_report(y_test, y_pred, target_names=le.classes_, zero_division=0))

        cm = confusion_matrix(y_test, y_pred)
        plt.figure(figsize=(max(5, len(le.classes_) + 2), max(4, len(le.classes_) + 1)))
        sns.heatmap(cm, annot=True, fmt="d", cmap="Blues",
                    xticklabels=le.classes_, yticklabels=le.classes_)
        plt.title(f"{name} — {title}")
        plt.xlabel("Predicted")
        plt.ylabel("Actual")
        plt.tight_layout()
        plt.savefig(os.path.join(OUTPUT_DIR,
            f"cm_{slug}_{name.lower().replace(' ', '_')}.png"), dpi=150)
        plt.close()

    ranking = pd.DataFrame(results).T.sort_values("f1_macro", ascending=False)
    print(f"\nRanking — {title}:")
    print(ranking.to_string())
    ranking.to_csv(os.path.join(OUTPUT_DIR, f"ranking_{slug}.csv"))

    rf = MODELS["Random Forest"]
    cv = cross_val_score(rf, X, y, cv=5, scoring="f1_macro")
    print(f"\nCV F1 Random Forest (5-fold): {cv.mean():.4f} ± {cv.std():.4f}")

    if hasattr(rf, "feature_importances_"):
        fi = pd.DataFrame({
            "feature":    available,
            "importance": rf.feature_importances_,
        }).sort_values("importance", ascending=False)

        print(f"\nFeature Importance — {title}:")
        print(fi.to_string(index=False))
        fi.to_csv(os.path.join(OUTPUT_DIR, f"fi_{slug}.csv"), index=False)

        plt.figure(figsize=(10, max(4, len(available) * 0.5)))
        sns.barplot(data=fi, x="importance", y="feature", palette="viridis")
        plt.title(f"Feature Importance — {title}")
        plt.tight_layout()
        plt.savefig(os.path.join(OUTPUT_DIR, f"fi_{slug}.png"), dpi=150)
        plt.close()


def main():
    mqtt_dfs, pcap_dfs = load_csvs()

    if mqtt_dfs:
        df_mqtt = prepare_mqtt(mqtt_dfs)
        print(f"\n[MQTT] {len(df_mqtt)} rows after cleaning")
        run(df_mqtt, MQTT_FEATURES, "MQTT Payload")
    else:
        print("\n[!] No MQTT CSVs found")

    if pcap_dfs:
        df_pcap = pd.concat(pcap_dfs, ignore_index=True)
        if "label" in df_pcap.columns:
            df_pcap["label"] = df_pcap["label"].str.strip().replace(
                {"benig": "benign", "legitimate": "benign", "normal": "benign"}
            )
        print(f"\n[PCAP] {len(df_pcap)} total rows")
        run(df_pcap, PCAP_FEATURES, "PCAP Flow")
    else:
        print("\n[!] No PCAP CSVs found")

    print(f"\nResults saved to ./{OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
