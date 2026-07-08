"""
K-means clustering pipeline — Lock-In Pomodoro
------------------------------------------------
Local dev:  functions-framework --target=run_kmeans --port=8080
Deploy:     firebase deploy --only functions  (from cloud/ directory)

Pipeline per device:
  1. Fetch sessions from last 30 days from Firestore (REST, no auth needed)
  2. Silhouette-scored K selection (K=2..4) on hour-of-day
  3. Label clusters by centroid hour (morning/afternoon/evening/night)
  4. Compute centroid stats and write to devices/{deviceId}/insights/kmeans
"""

import json
import logging
import os
from collections import Counter
from datetime import datetime, timedelta

import functions_framework
import numpy as np
import pytz
import requests
from sklearn.cluster import KMeans
from sklearn.metrics import silhouette_score

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger(__name__)

# ── Config ────────────────────────────────────────────────────────────────────
FIRESTORE_PROJECT = os.environ.get("FIRESTORE_PROJECT", "lock-in-81e21")
FIRESTORE_BASE = (
    f"https://firestore.googleapis.com/v1/projects/{FIRESTORE_PROJECT}"
    f"/databases/(default)/documents"
)
TZ           = pytz.timezone("Asia/Kuala_Lumpur")
LOOKBACK_DAYS = 30
K_MIN, K_MAX  = 2, 4   # silhouette score picks the best K in this range


# ── Firestore REST helpers ────────────────────────────────────────────────────

def _parse_value(v: dict):
    if "stringValue"    in v: return v["stringValue"]
    if "integerValue"   in v: return int(v["integerValue"])
    if "doubleValue"    in v: return float(v["doubleValue"])
    if "booleanValue"   in v: return bool(v["booleanValue"])
    if "nullValue"      in v: return None
    if "timestampValue" in v: return v["timestampValue"]
    if "mapValue"       in v:
        return {k: _parse_value(fv) for k, fv in v["mapValue"]["fields"].items()}
    if "arrayValue"     in v:
        return [_parse_value(av) for av in v["arrayValue"].get("values", [])]
    return None

def _parse_doc(doc: dict) -> dict:
    return {k: _parse_value(v) for k, v in doc.get("fields", {}).items()}

def _to_value(val) -> dict:
    if isinstance(val, bool):  return {"booleanValue": val}
    if isinstance(val, int):   return {"integerValue": str(val)}
    if isinstance(val, float): return {"doubleValue": val}
    if isinstance(val, str):   return {"stringValue": val}
    if isinstance(val, list):
        return {"arrayValue": {"values": [_to_value(v) for v in val]}}
    if isinstance(val, dict):
        return {"mapValue": {"fields": {k: _to_value(v) for k, v in val.items()}}}
    if val is None: return {"nullValue": None}
    return {"stringValue": str(val)}

def _firestore_body(data: dict) -> dict:
    return {"fields": {k: _to_value(v) for k, v in data.items()}}

def list_devices() -> list[str]:
    resp = requests.get(f"{FIRESTORE_BASE}/devices", timeout=20)
    resp.raise_for_status()
    return [d["name"].split("/")[-1] for d in resp.json().get("documents", [])]

def fetch_sessions(device_id: str) -> list[dict]:
    url    = f"{FIRESTORE_BASE}/devices/{device_id}/sessions"
    docs   = []
    params = {"pageSize": 300}
    while True:
        resp = requests.get(url, params=params, timeout=30)
        resp.raise_for_status()
        body = resp.json()
        docs.extend(body.get("documents", []))
        token = body.get("nextPageToken")
        if not token:
            break
        params["pageToken"] = token
    return [_parse_doc(d) for d in docs]

def write_insights(device_id: str, data: dict):
    url    = f"{FIRESTORE_BASE}/devices/{device_id}/insights/kmeans"
    params = [("updateMask.fieldPaths", f) for f in data.keys()]
    resp   = requests.patch(url, json=_firestore_body(data), params=params, timeout=20)
    resp.raise_for_status()


# ── Date / time helpers ───────────────────────────────────────────────────────

def _parse_date(s: str):
    try:    return datetime.strptime(s, "%d/%m/%Y").date()
    except: return None

def _parse_hour(s: str) -> float | None:
    try:
        h, m = map(int, s.split(":"))
        return h + m / 60.0
    except:
        return None

def _hour_to_label(h: float) -> str:
    """Map a centroid hour to a human-readable time-of-day label."""
    if  5 <= h < 12: return "morning"
    if 12 <= h < 18: return "afternoon"
    if 18 <= h < 24: return "evening"
    return "night"                        # midnight–05:00


# ── K selection via silhouette score ─────────────────────────────────────────

def _best_k(hours: np.ndarray) -> tuple[int, dict[int, float]]:
    """
    Try K = K_MIN..K_MAX, return the K with the highest average silhouette score
    and a dict of all scores for transparency.

    Silhouette score needs at least 2 clusters and 2 samples per cluster.
    Falls back to K_MIN if scoring fails for all candidates.
    """
    scores: dict[int, float] = {}
    n = len(hours)

    for k in range(K_MIN, min(K_MAX, n - 1) + 1):
        try:
            km     = KMeans(n_clusters=k, n_init=10, random_state=42)
            labels = km.fit_predict(hours)
            # silhouette_score requires at least 2 distinct labels
            if len(set(labels)) < 2:
                continue
            scores[k] = float(silhouette_score(hours, labels))
        except Exception as e:
            log.warning(f"Silhouette failed for K={k}: {e}")

    if not scores:
        log.warning("All silhouette attempts failed — defaulting to K_MIN")
        return K_MIN, {}

    best = max(scores, key=scores.get)
    log.info("Silhouette scores: " + ", ".join(f"K={k}: {v:.3f}" for k, v in sorted(scores.items()))
             + f"  →  chosen K={best}")
    return best, scores


# ── K-means pipeline ──────────────────────────────────────────────────────────

def run_pipeline_for_device(device_id: str) -> dict:
    now_kl = datetime.now(TZ)
    cutoff = (now_kl - timedelta(days=LOOKBACK_DAYS)).date()

    raw = fetch_sessions(device_id)
    log.info(f"[{device_id}] Fetched {len(raw)} total sessions")

    sessions = []
    for s in raw:
        date = _parse_date(s.get("sesstionStartDate", ""))
        hour = _parse_hour(s.get("sessionStartTime", ""))
        if date is None or date < cutoff or hour is None:
            continue
        sessions.append({
            "date":           s.get("sesstionStartDate", ""),
            "time":           s.get("sessionStartTime", ""),
            "hour":           hour,
            "total_work_sec": int(s.get("totalWorkSec",    0) or 0),
            "total_rest_sec": int(s.get("totalRestSec",    0) or 0),
            "avg_work_sec":   int(s.get("avgWorkSec",      0) or 0),
            "avg_rest_sec":   int(s.get("avgRestSec",      0) or 0),
            "num_rounds":     int(s.get("numberofRounds",  0) or 0),
        })

    n = len(sessions)
    log.info(f"[{device_id}] {n} sessions in last {LOOKBACK_DAYS} days")

    if n < K_MIN * 2:
        msg = f"Only {n} sessions — need at least {K_MIN * 2}"
        log.warning(f"[{device_id}] {msg}")
        return {"status": "skipped", "reason": msg, "sessions_found": n}

    # ── Pick best K via silhouette score ──
    hours        = np.array([[s["hour"]] for s in sessions])
    chosen_k, sil_scores = _best_k(hours)

    # ── Fit final model with chosen K ──
    kmeans   = KMeans(n_clusters=chosen_k, n_init=10, random_state=42)
    raw_lbls = kmeans.fit_predict(hours)
    centroids = kmeans.cluster_centers_.flatten()

    # ── Assign time-of-day labels by centroid hour ──
    # Sort cluster indices by centroid hour so labels are assigned chronologically.
    # Handle collisions (two clusters in the same time window) with a numeric suffix.
    sorted_idx  = np.argsort(centroids)
    seen_labels: dict[str, int] = {}
    idx_to_label: dict[int, str] = {}

    for ci in sorted_idx:
        base  = _hour_to_label(float(centroids[ci]))
        count = seen_labels.get(base, 0)
        seen_labels[base] = count + 1
        idx_to_label[int(ci)] = base if count == 0 else f"{base}_{count + 1}"

    # ── Compute per-cluster stats ──
    groups: dict[str, list] = {lbl: [] for lbl in idx_to_label.values()}
    for i, s in enumerate(sessions):
        groups[idx_to_label[int(raw_lbls[i])]].append(s)

    clusters_out: dict[str, dict] = {}
    for label, grp in groups.items():
        if not grp:
            clusters_out[label] = {"label": label, "session_count": 0}
            continue
        avg_hour = float(np.mean([s["hour"] for s in grp]))
        h, m     = int(avg_hour), int((avg_hour % 1) * 60)
        rounds   = [s["num_rounds"] for s in grp]
        clusters_out[label] = {
            "label":                  label,
            "centroid_hour":          round(avg_hour, 2),
            "avg_start_time":         f"{h:02d}:{m:02d}",
            "avg_total_work_min":     round(float(np.mean([s["total_work_sec"] for s in grp])) / 60, 1),
            "avg_total_rest_min":     round(float(np.mean([s["total_rest_sec"] for s in grp])) / 60, 1),
            "avg_work_per_round_min": round(float(np.mean([s["avg_work_sec"]   for s in grp])) / 60, 1),
            "avg_rounds":             round(float(np.mean(rounds)), 1),
            "mode_rounds":            int(Counter(rounds).most_common(1)[0][0]),
            "session_count":          len(grp),
        }

    # ── Session points for scatter plot ──
    session_points = [
        {
            "date":           s["date"],
            "time":           s["time"],
            "hour":           round(s["hour"], 2),
            "total_work_min": round(s["total_work_sec"] / 60, 1),
            "num_rounds":     s["num_rounds"],
            "cluster":        idx_to_label[int(raw_lbls[i])],
        }
        for i, s in enumerate(sessions)
    ]

    result = {
        "clusters":        clusters_out,
        "session_points":  session_points,
        "model": {
            "chosen_k":         chosen_k,
            "silhouette_scores": {str(k): round(v, 4) for k, v in sil_scores.items()},
            "best_silhouette":   round(sil_scores.get(chosen_k, 0.0), 4),
        },
        "computed_at":     now_kl.isoformat(),
        "sessions_used":   n,
        "date_range": {
            "from": cutoff.strftime("%d/%m/%Y"),
            "to":   now_kl.date().strftime("%d/%m/%Y"),
        },
    }

    write_insights(device_id, result)
    log.info(f"[{device_id}] ✓ K={chosen_k}  silhouette={sil_scores.get(chosen_k, 0):.3f}  "
             + "  ".join(f"{lbl}={clusters_out[lbl]['session_count']}" for lbl in clusters_out))

    return {"status": "ok", "device_id": device_id,
            **{k: v for k, v in result.items() if k != "session_points"}}


# ── HTTP entry point ──────────────────────────────────────────────────────────

@functions_framework.http
def run_kmeans(request):
    """
    POST /           → run pipeline for all devices
    POST /?device=X  → run pipeline for one device only
    GET  /           → health check
    """
    if request.method == "GET":
        return (json.dumps({"status": "ok", "message": "K-means pipeline ready",
                            "k_range": f"{K_MIN}–{K_MAX}"}),
                200, {"Content-Type": "application/json"})

    device_filter = request.args.get("device")
    try:
        devices = [device_filter] if device_filter else list_devices()
        log.info(f"Running pipeline for {len(devices)} device(s): {devices}")
        results = {dev: run_pipeline_for_device(dev) for dev in devices}
        return (json.dumps({"status": "ok", "results": results}, default=str),
                200, {"Content-Type": "application/json"})
    except Exception as exc:
        log.error("Pipeline error", exc_info=True)
        return (json.dumps({"status": "error", "message": str(exc)}),
                500, {"Content-Type": "application/json"})


# ── Production scheduled trigger (uncomment when deploying) ──────────────────
#
# from firebase_functions import scheduler_fn
#
# @scheduler_fn.on_schedule(
#     schedule="every 24 hours",
#     timezone=scheduler_fn.Timezone("Asia/Kuala_Lumpur"),
# )
# def daily_kmeans(event: scheduler_fn.ScheduledEvent) -> None:
#     for dev_id in list_devices():
#         run_pipeline_for_device(dev_id)
