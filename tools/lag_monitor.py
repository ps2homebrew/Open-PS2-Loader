#!/usr/bin/env python3
"""
Input lag monitor for the OPL-DualSense-BT debug build.

The debug ELF prints one "LAT2" line per second with timing measured inside the
PS2 (DualSense on port 1). This script listens for those logs (UDP port 18194,
same as ps2_log.py), shows a live summary and prints a report when you stop it.

Usage:
  python lag_monitor.py               # live, listening to the PS2
  python lag_monitor.py --file X.txt  # analyze a saved log (ps2_log.txt / lag_log.txt)

Everything received is also saved to lag_log.txt.

What it measures (all inside the PS2, not the TV):
  transport  time a report spends between the DualSense and our driver (Bluetooth air + USB +
             queues), relative to the best case seen in the session
  gap        longest pause between two reports in that second (stalls)
  press->game  time from a button change arriving to the game reading it (includes waiting
             for the game's next frame, which also happens with a wired pad)
  age        how old the newest report is when the game reads the pad
  cb         time our driver spends handling each report
  tx         output reports sent to the pad per second (rumble/LED)
"""
import argparse
import re
import socket
import statistics
import sys
import time

PORT = 18194
LAT2 = re.compile(r"LAT2 (.*)")
WINDOW = 30  # seconds used to estimate the "best case" transport time (handles clock drift)


def parse(line):
    m = LAT2.search(line)
    if not m:
        return None
    d = {}
    for kv in m.group(1).split():
        k, _, v = kv.partition("=")
        try:
            d[k] = int(v)
        except ValueError:
            pass
    return d if "rx" in d else None


class Stats:
    def __init__(self):
        self.rows = []

    def add(self, d):
        # baseline = lowest clock offset in the last WINDOW seconds
        self.rows.append(d)
        recent = [r["off_min"] for r in self.rows[-WINDOW:] if r.get("rx", 0) > 0]
        base = min(recent) if recent else d["off_min"]
        d["tr_min"] = (d["off_min"] - base) / 1000.0
        d["tr_max"] = (d["off_max"] - base) / 1000.0
        return d

    @staticmethod
    def line(d):
        chg = (f"press->game {d['chg_avg']/1000:5.1f} avg {d['chg_max']/1000:5.1f} max (n={d['chg']})"
               if d.get("chg") else "press->game   -  (no presses)")
        return (f"t={d['t']//1000:4d}s  rx={d['rx']:4d}/s  gap={d['gap']/1000:5.1f}ms  "
                f"transport={d['tr_min']:4.1f}..{d['tr_max']:5.1f}ms  "
                f"game={d['polls']:3d}Hz age={d['age_avg']/1000:4.1f}/{d['age_max']/1000:4.1f}ms  "
                f"cb={d['cb_avg']}/{d['cb_max']}us  tx={d['tx']}/s  {chg}")

    def report(self):
        rows = [r for r in self.rows if r.get("rx", 0) > 0]
        if not rows:
            print("\nNo LAT2 data. Is the debug ELF running and the DualSense connected on port 1?")
            return
        rows = rows[2:] or rows  # skip the first seconds (connection settling)

        def pct(vals, p):
            vals = sorted(vals)
            return vals[min(len(vals) - 1, int(round(p / 100 * (len(vals) - 1))))]

        def show(name, vals, unit="ms"):
            if not vals:
                print(f"  {name:<34} -")
                return
            print(f"  {name:<34} median {statistics.median(vals):6.1f}   p95 {pct(vals, 95):6.1f}   max {max(vals):6.1f} {unit}")

        print("\n================ INPUT LAG REPORT ================")
        print(f"  seconds analyzed: {len(rows)}")
        show("reports per second", [r["rx"] for r in rows], "/s")
        show("game reads per second", [r["polls"] for r in rows], "/s")
        show("longest pause between reports", [r["gap"] / 1000 for r in rows])
        show("transport (worst per second)", [r["tr_max"] for r in rows])
        show("report age when game reads", [r["age_max"] / 1000 for r in rows])
        show("driver time per report", [r["cb_avg"] / 1000 for r in rows])
        show("output reports per second", [r["tx"] for r in rows], "/s")
        presses = [r for r in rows if r.get("chg")]
        show("press -> game read (avg)", [r["chg_avg"] / 1000 for r in presses])
        show("press -> game read (worst)", [r["chg_max"] / 1000 for r in presses])

        print("\n  What it means:")
        gaps = [r["gap"] / 1000 for r in rows]
        trs = [r["tr_max"] for r in rows]
        bad_gap = sum(1 for g in gaps if g > 20)
        bad_tr = sum(1 for t in trs if t > 15)
        frame = 1000 / max(1, statistics.median([r["polls"] for r in rows]))
        if presses:
            print(f"  - The game reads the pad every ~{frame:.1f} ms; any pad (even wired) waits up to that.")
        if bad_gap:
            print(f"  - {bad_gap} second(s) had pauses over 20 ms between reports: the pad or the USB bus stalled.")
            print("    If it happens while the game loads data, it's the USB stick sharing the bus with the dongle.")
        else:
            print("  - No stalls over 20 ms between reports.")
        if bad_tr:
            print(f"  - {bad_tr} second(s) had reports arriving over 15 ms later than the best case (radio/USB queuing).")
        else:
            print("  - Transport stayed within 15 ms of the best case.")
        tx = statistics.median([r["tx"] for r in rows])
        if tx > 10:
            print(f"  - {tx:.0f} output reports/s sent to the pad: that's a lot of radio traffic (old build?).")
        print("==================================================")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--file", help="analyze a saved log instead of listening")
    a = ap.parse_args()
    st = Stats()

    if a.file:
        with open(a.file, encoding="utf-8", errors="replace") as f:
            for line in f:
                d = parse(line)
                if d:
                    st.add(d)
        st.report()
        return

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", PORT))
    log = open("lag_log.txt", "a", encoding="utf-8")
    log.write(f"\n===== session {time.strftime('%Y-%m-%d %H:%M:%S')} =====\n")
    print(f"Listening on UDP {PORT}. Start a game, press PS on the DualSense and play. Ctrl+C for the report.")
    buf = ""
    try:
        while True:
            data, _ = sock.recvfrom(4096)
            buf += data.decode("latin-1", errors="replace")
            while "\n" in buf:
                line, buf = buf.split("\n", 1)
                log.write(line.rstrip() + "\n")
                d = parse(line)
                if d:
                    print(Stats.line(st.add(d)))
            log.flush()
    except KeyboardInterrupt:
        st.report()


if __name__ == "__main__":
    main()
