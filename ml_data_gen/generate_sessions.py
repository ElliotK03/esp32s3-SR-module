"""
Mock Pomodoro session generator — pushes ~100 sessions to Firestore.

The ESP32 firmware hits Firestore without authentication (open rules),
so this script does the same — no token needed.

Usage:
    pip install requests
    python generate_sessions.py
"""

import uuid
import random
import time
import datetime
import requests

# ── Config ───────────────────────────────────────────────────────────────────
FIRESTORE_PROJECT  = "lock-in-81e21"
DEVICE_ID          = "14C19F442534"

NUM_SESSIONS       = 100

# Realistic work/break durations (seconds) that a pomodoro user would choose
WORK_CHOICES_SEC   = [15*60, 20*60, 25*60, 30*60, 45*60]   # 15–45 min
BREAK_CHOICES_SEC  = [5*60, 10*60, 15*60]                   # 5–15 min

# Spread sessions over the past 60 days, between 07:00–23:00
DAYS_BACK          = 60
SESSION_START_HOUR = 7
SESSION_END_HOUR   = 23

# ── Helpers ──────────────────────────────────────────────────────────────────

def random_datetime() -> datetime.datetime:
    """Return a random datetime within working hours over the last DAYS_BACK days."""
    now = datetime.datetime.now()
    day_offset = random.randint(0, DAYS_BACK)
    base = now - datetime.timedelta(days=day_offset)
    hour = random.randint(SESSION_START_HOUR, SESSION_END_HOUR - 1)
    minute = random.choice([0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55])
    return base.replace(hour=hour, minute=minute, second=0, microsecond=0)


def generate_session_id() -> str:
    return str(uuid.uuid4())


def build_rounds_str(rounds: list[dict]) -> str:
    """Reproduce the exact multiline string the firmware generates."""
    parts = [" \n"]
    for i, r in enumerate(rounds):
        parts.append(f"        {i}: {{{r['work_duration']}, {r['break_duration']}}}\n")
    parts.append("    ")
    return "".join(parts)


def build_firestore_body(
    start_date: str,
    start_time: str,
    num_rounds: int,
    rounds_str: str,
    total_work: int,
    avg_work: int,
    total_rest: int,
    avg_rest: int,
) -> dict:
    """Build the Firestore REST API document body."""
    def int_val(v):  return {"integerValue": str(v)}
    def str_val(v):  return {"stringValue": str(v)}

    return {
        "fields": {
            "sesstionStartDate":  str_val(start_date),   # firmware typo preserved
            "sessionStartTime":   str_val(start_time),
            "numberofRounds":     int_val(num_rounds),
            "rounds":             str_val(rounds_str),
            "totalWorkSec":       int_val(total_work),
            "avgWorkSec":         int_val(avg_work),
            "totalRestSec":       int_val(total_rest),
            "avgRestSec":         int_val(avg_rest),
        }
    }


def push_session(session_id: str, body: dict) -> bool:
    """POST to Firestore REST API (no auth — mirrors firmware behaviour)."""
    url = (
        f"https://firestore.googleapis.com/v1/projects/{FIRESTORE_PROJECT}"
        f"/databases/(default)/documents/devices/{DEVICE_ID}/sessions"
        f"?documentId={session_id}"
    )
    headers = {"Content-Type": "application/json"}
    resp = requests.post(url, headers=headers, json=body, timeout=15)
    if resp.status_code in (200, 201):
        return True
    print(f"  ERROR {resp.status_code}: {resp.text[:200]}")
    return False


# ── Session generation model ──────────────────────────────────────────────────

def generate_session() -> dict:
    """
    Generate one plausible pomodoro session.

    Behaviour modelled:
    - 1–6 rounds per session
    - Work duration chosen from realistic presets with some noise
    - Rest duration always shorter than work
    - Sessions later in the day tend to be shorter (fatigue)
    - ~20 % of rounds have a truncated work time (user stopped early)
    """
    num_rounds = random.choices([1, 2, 3, 4, 5, 6], weights=[5, 15, 30, 25, 15, 10])[0]

    base_work = random.choice(WORK_CHOICES_SEC)
    base_rest = random.choice([s for s in BREAK_CHOICES_SEC if s < base_work])

    rounds = []
    for _ in range(num_rounds):
        # ±10 % noise on work; occasional early stop (60–95 % of planned)
        if random.random() < 0.20:
            work = int(base_work * random.uniform(0.60, 0.95))
        else:
            work = int(base_work * random.uniform(0.90, 1.05))
        work = max(60, work)  # at least 1 min

        # Last round often has no rest
        if _ == num_rounds - 1:
            rest = 0
        else:
            rest = int(base_rest * random.uniform(0.85, 1.15))
            rest = max(60, rest)

        rounds.append({"work_duration": work, "break_duration": rest})

    total_work = sum(r["work_duration"] for r in rounds)
    total_rest = sum(r["break_duration"] for r in rounds)
    avg_work   = total_work // num_rounds
    avg_rest   = total_rest // num_rounds if num_rounds > 0 else 0

    dt = random_datetime()
    start_date = dt.strftime("%d/%m/%Y")
    start_time = dt.strftime("%H:%M")

    return {
        "session_id":  generate_session_id(),
        "start_date":  start_date,
        "start_time":  start_time,
        "num_rounds":  num_rounds,
        "rounds":      rounds,
        "total_work":  total_work,
        "avg_work":    avg_work,
        "total_rest":  total_rest,
        "avg_rest":    avg_rest,
    }


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    print(f"Generating {NUM_SESSIONS} sessions for device {DEVICE_ID}...")
    ok = 0
    fail = 0

    for i in range(NUM_SESSIONS):
        s = generate_session()
        rounds_str = build_rounds_str(s["rounds"])
        body = build_firestore_body(
            start_date = s["start_date"],
            start_time = s["start_time"],
            num_rounds = s["num_rounds"],
            rounds_str = rounds_str,
            total_work = s["total_work"],
            avg_work   = s["avg_work"],
            total_rest = s["total_rest"],
            avg_rest   = s["avg_rest"],
        )

        success = push_session(s["session_id"], body)
        status = "✓" if success else "✗"
        print(f"  [{i+1:3d}/{NUM_SESSIONS}] {status}  {s['start_date']} {s['start_time']}  "
              f"{s['num_rounds']} rounds  work={s['total_work']//60}min  rest={s['total_rest']//60}min")

        if success:
            ok += 1
        else:
            fail += 1

        # Avoid hammering the API — Firestore free tier has write limits
        time.sleep(0.3)

    print(f"\nDone. {ok} pushed, {fail} failed.")


if __name__ == "__main__":
    main()
