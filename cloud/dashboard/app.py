"""
Lock-In Pomodoro — K-means insights dashboard (local dev)
Run:  python app.py
      Open: http://localhost:5000
"""

import logging
import os

import pytz
import requests
from flask import Flask, jsonify, render_template

logging.basicConfig(level=logging.INFO)
log = logging.getLogger(__name__)

app = Flask(__name__)

FIRESTORE_PROJECT = os.environ.get("FIRESTORE_PROJECT", "lock-in-81e21")
FIRESTORE_BASE = (
    f"https://firestore.googleapis.com/v1/projects/{FIRESTORE_PROJECT}"
    f"/databases/(default)/documents"
)
PIPELINE_URL = os.environ.get("PIPELINE_URL", "http://localhost:8080/")


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


# ── API routes ────────────────────────────────────────────────────────────────

@app.route("/api/devices")
def api_devices():
    try:
        resp = requests.get(f"{FIRESTORE_BASE}/devices", timeout=10)
        resp.raise_for_status()
        devices = [d["name"].split("/")[-1] for d in resp.json().get("documents", [])]
        return jsonify({"devices": devices})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/insights/<device_id>")
def api_insights(device_id: str):
    try:
        url = f"{FIRESTORE_BASE}/devices/{device_id}/insights/kmeans"
        resp = requests.get(url, timeout=10)
        if resp.status_code == 404:
            return jsonify({"error": "No insights yet — run the pipeline first."}), 404
        resp.raise_for_status()
        data = _parse_doc(resp.json())
        return jsonify(data)
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/run", methods=["POST"])
@app.route("/api/run/<device_id>", methods=["POST"])
def api_run(device_id: str | None = None):
    """Proxy to the local pipeline function server."""
    try:
        url = PIPELINE_URL + (f"?device={device_id}" if device_id else "")
        resp = requests.post(url, timeout=120)
        return (resp.text, resp.status_code, {"Content-Type": "application/json"})
    except requests.exceptions.ConnectionError:
        return jsonify({"error": f"Pipeline server not reachable at {PIPELINE_URL}"}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/")
def index():
    return render_template("index.html")


if __name__ == "__main__":
    app.run(debug=True, port=5000, host="0.0.0.0")
