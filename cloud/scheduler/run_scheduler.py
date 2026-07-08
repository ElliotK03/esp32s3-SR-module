"""
Local daily scheduler — triggers the K-means pipeline every day at midnight KL time.
Also fires once on startup so you see output immediately.

Usage:  python run_scheduler.py
        python run_scheduler.py --now          # fire immediately then exit
        python run_scheduler.py --device <id>  # target a single device
"""

import argparse
import logging
import sys
import time
from datetime import datetime

import pytz
import requests
import schedule

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger(__name__)

PIPELINE_URL = "http://localhost:8080/"
TZ = pytz.timezone("Asia/Kuala_Lumpur")


def trigger(device: str | None = None):
    now = datetime.now(TZ).strftime("%Y-%m-%d %H:%M:%S %Z")
    log.info(f"Triggering K-means pipeline at {now}" + (f" for device {device}" if device else ""))
    try:
        url = PIPELINE_URL + (f"?device={device}" if device else "")
        resp = requests.post(url, timeout=120)
        if resp.status_code == 200:
            data = resp.json()
            for dev_id, result in data.get("results", {}).items():
                status = result.get("status", "?")
                used   = result.get("sessions_used", 0)
                log.info(f"  [{dev_id}] {status} — {used} sessions processed")
                if "clusters" in result:
                    for lbl, c in result["clusters"].items():
                        log.info(f"    {lbl:10s}: {c.get('session_count', 0):3d} sessions  "
                                 f"avg start {c.get('avg_start_time', '?')}  "
                                 f"avg work {c.get('avg_total_work_min', 0):.1f} min")
        else:
            log.error(f"Pipeline returned HTTP {resp.status_code}: {resp.text[:300]}")
    except requests.exceptions.ConnectionError:
        log.error(f"Could not reach pipeline at {PIPELINE_URL} — is it running?")
    except Exception as exc:
        log.error(f"Trigger failed: {exc}", exc_info=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--now",    action="store_true", help="Fire once and exit")
    parser.add_argument("--device", default=None,        help="Target a single device ID")
    args = parser.parse_args()

    if args.now:
        trigger(args.device)
        sys.exit(0)

    # Fire on startup
    trigger(args.device)

    # Schedule daily at midnight KL — adjust the time string if your machine clock differs
    schedule.every().day.at("00:00").do(trigger, device=args.device)
    log.info("Scheduler running — next pipeline run at 00:00 local time (KL midnight)")
    log.info("Press Ctrl+C to stop")

    while True:
        schedule.run_pending()
        time.sleep(30)


if __name__ == "__main__":
    main()
