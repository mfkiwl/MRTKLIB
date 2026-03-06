#!/usr/bin/env python3
"""compare_nmea_abs.py - Absolute accuracy check of NMEA GGA output vs geodetic truth.

Compares a NMEA GGA file against a true reference coordinate derived from
either an IGS SINEX file or a GSI F5 daily coordinate file.  Shares all
reference-parsing and pass/fail logic with compare_pos_abs.py.

Reference derivation and pass/fail criteria are identical to compare_pos_abs.py.
See that script's docstring for details.

Height note
-----------
NMEA GGA altitude is height above the geoid (orthometric height), while SINEX
and GSI F5 provide ellipsoidal (GRS80/ITRF) height.  The resulting Up-error
includes the geoid undulation at the test site (≈ 30–50 m in Japan), which
makes 3D accuracy unreliable for absolute comparison.

For robust absolute checks use the horizontal-only (2D) metric: the 1σ and
95% criteria are evaluated against 2D horizontal error unless --use-3d is
specified.  3D statistics are always printed for reference.

Usage
-----
    compare_nmea_abs.py --sinex FILE.SNX[.gz] --station CODE [--epoch YYYY/MM/DD]
                        [options] test.nmea
    compare_nmea_abs.py --f5 FILE --date YYYY/MM/DD
                        [options] test.nmea

Options
-------
    --tolerance FLOAT   Tolerance for criterion A in metres (default 0.100)
    --skip-epochs INT   Skip N initial epochs
    --use-3d            Evaluate pass/fail on 3D error instead of 2D horizontal
    --plot              Generate ENU error time-series plot
"""

import argparse
import math
import os
import sys

import numpy as np

# Import shared geometry and reference-parsing helpers from compare_pos_abs.py
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from compare_pos_abs import (   # noqa: E402
    blh2xyz, xyz2blh, xyz2enu,
    parse_sinex, sinex_propagate,
    parse_f5,
    _criterion,
)


# ---------------------------------------------------------------------------
# NMEA GGA parser  (adapted from compare_nmea.py)
# ---------------------------------------------------------------------------
def _nmea_to_deg(val_str, hemi):
    """Convert NMEA DDmm.mmmm / DDDmm.mmmm to decimal degrees."""
    val = float(val_str)
    deg = int(val / 100)
    minutes = val - deg * 100
    result = deg + minutes / 60.0
    if hemi in ("S", "W"):
        result = -result
    return result


def parse_nmea(filepath):
    """Parse NMEA GGA sentences.

    Args:
        filepath: Path to NMEA file.

    Returns:
        dict mapping time-key (HHMMSS.ss string) to (lat_deg, lon_deg, alt, quality).
        alt is orthometric height above the geoid as reported in the GGA sentence.
    """
    data = {}
    with open(filepath) as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue
            if "*" in line:
                line = line[: line.index("*")]
            fields = line.split(",")
            if len(fields) < 10:
                continue
            if fields[0] not in ("$GPGGA", "$GNGGA"):
                continue
            try:
                time_key = fields[1]
                quality = int(fields[6])
                if quality == 0 or not fields[2] or not fields[4]:
                    continue
                lat = _nmea_to_deg(fields[2], fields[3])
                lon = _nmea_to_deg(fields[4], fields[5])
                alt = float(fields[9])
                data[time_key] = (lat, lon, alt, quality)
            except (ValueError, IndexError):
                continue
    return data


# ---------------------------------------------------------------------------
# Absolute accuracy metrics  (horizontal + 3D)
# ---------------------------------------------------------------------------
def compute_abs_metrics(true_xyz, test_data, skip_epochs=0):
    """Compute per-epoch position errors against a fixed true coordinate.

    Because NMEA altitude is orthometric (above geoid) rather than ellipsoidal,
    the true_xyz coordinate is used only for its lat/lon when projecting to ENU.
    Horizontal (East, North) errors are geoid-independent; Up errors may be
    biased by the geoid undulation at the site.

    Args:
        true_xyz: np.array([X, Y, Z]) — true ECEF coordinate in metres.
        test_data: dict from parse_nmea().
        skip_epochs: initial epochs to discard.

    Returns:
        dict of statistics, or None if no usable epochs.
    """
    true_lat, true_lon, true_h_ell = xyz2blh(*true_xyz)
    epochs = sorted(test_data.keys())[skip_epochs:]
    if not epochs:
        return None

    enu_errors, q_list = [], []
    for key in epochs:
        lat, lon, alt_orth, q = test_data[key]
        # Use ellipsoidal height of reference for ECEF conversion of test point
        # (avoids inflating Up error by the full geoid undulation)
        test_xyz = blh2xyz(lat, lon, true_h_ell)   # use ref ellipsoidal h
        dx = test_xyz - true_xyz
        enu = xyz2enu(dx, true_lat, true_lon)
        enu_errors.append(enu)
        q_list.append(q)

    en = np.array(enu_errors)
    n = len(en)

    horiz = np.sqrt(en[:, 0] ** 2 + en[:, 1] ** 2)
    e3d   = np.sqrt(en[:, 0] ** 2 + en[:, 1] ** 2 + en[:, 2] ** 2)

    return {
        "n": n,
        "enu_errors": en,
        "q_list": q_list,
        "true_lat": true_lat,
        "true_lon": true_lon,
        # Horizontal (2D) — geoid-independent
        "mean_2d":  float(np.mean(horiz)),
        "rms_2d":   float(np.sqrt(np.mean(horiz ** 2))),
        "p68_2d":   float(np.percentile(horiz, 68)),
        "p95_2d":   float(np.percentile(horiz, 95)),
        "max_2d":   float(np.max(horiz)),
        "rms_e":    float(np.sqrt(np.mean(en[:, 0] ** 2))),
        "rms_n":    float(np.sqrt(np.mean(en[:, 1] ** 2))),
        # 3D — Up may include geoid bias; informational only unless --use-3d
        "mean_3d":  float(np.mean(e3d)),
        "rms_3d":   float(np.sqrt(np.mean(e3d ** 2))),
        "p68_3d":   float(np.percentile(e3d, 68)),
        "p95_3d":   float(np.percentile(e3d, 95)),
        "max_3d":   float(np.max(e3d)),
        "rms_u":    float(np.sqrt(np.mean(en[:, 2] ** 2))),
        "fix_rate": sum(1 for q in q_list if q in (1, 4)) / n * 100.0,
    }


# ---------------------------------------------------------------------------
# Plot
# ---------------------------------------------------------------------------
def plot_results(m, ref_label, output_path="abs_nmea_compare.png"):
    """Generate ENU error time-series and Q-flag plot."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    en = m["enu_errors"] * 100      # m → cm
    idx = np.arange(m["n"])

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    ax1.plot(idx, en[:, 0], label="East",  alpha=0.8, linewidth=0.8)
    ax1.plot(idx, en[:, 1], label="North", alpha=0.8, linewidth=0.8)
    ax1.plot(idx, en[:, 2], label="Up (may include geoid bias)",
             alpha=0.5, linewidth=0.8, linestyle="--")
    ax1.axhline(0, color="k", linewidth=0.5)
    ax1.set_ylabel("Position error [cm]")
    ax1.set_title(
        f"Absolute error vs {ref_label} — "
        f"2D RMS {m['rms_2d']*100:.2f} cm  |  "
        f"1σ(2D) {m['p68_2d']*100:.2f} cm  |  "
        f"95%(2D) {m['p95_2d']*100:.2f} cm"
    )
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    ax2.scatter(idx, m["q_list"], s=8, alpha=0.6)
    ax2.set_ylabel("GGA quality")
    ax2.set_xlabel("Epoch")
    ax2.set_title(f"GGA quality  (fix rate {m['fix_rate']:.1f}%,  Q=1 or Q=4)")
    ax2.set_yticks([1, 2, 4, 5])
    ax2.set_yticklabels(["1:GPS", "2:DGPS", "4:Fix", "5:Float"])
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    print(f"  Plot saved: {output_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main():  # noqa: D103
    p = argparse.ArgumentParser(
        description="Absolute accuracy check of NMEA GGA output vs geodetic truth"
    )
    ref = p.add_mutually_exclusive_group(required=True)
    ref.add_argument("--sinex", metavar="FILE",
                     help="IGS SINEX file (.SNX or .SNX.gz)")
    ref.add_argument("--f5", metavar="FILE",
                     help="GSI F5 daily coordinate file")
    p.add_argument("--station", metavar="CODE",
                   help="4-char station code (required with --sinex)")
    p.add_argument("--date", metavar="YYYY/MM/DD",
                   help="Evaluation date for F5 15-day window (required with --f5)")
    p.add_argument("--epoch", metavar="YYYY/MM/DD",
                   help="Target epoch for SINEX propagation")
    p.add_argument("--tolerance", type=float, default=0.100,
                   help="Tolerance for criterion A in metres (default 0.100)")
    p.add_argument("--skip-epochs", type=int, default=0,
                   help="Initial epochs to discard")
    p.add_argument("--use-3d", action="store_true",
                   help="Evaluate pass/fail on 3D error (default: 2D horizontal)")
    p.add_argument("--plot", action="store_true",
                   help="Generate ENU error time-series plot")
    p.add_argument("test", help="NMEA GGA file to evaluate")
    args = p.parse_args()

    from datetime import datetime

    # ── Resolve true reference coordinate ───────────────────────────────────
    if args.sinex:
        if not args.station:
            print("FAIL: --station is required with --sinex", file=sys.stderr)
            return 1
        if not os.path.isfile(args.sinex):
            print(f"FAIL: SINEX file not found: {args.sinex}", file=sys.stderr)
            return 1
        sinex = parse_sinex(args.sinex, args.station)
        if args.epoch:
            target_dt = datetime.strptime(args.epoch, "%Y/%m/%d")
            true_xyz = sinex_propagate(sinex, target_dt)
            epoch_note = f"propagated to {args.epoch}"
        else:
            true_xyz = sinex["xyz"]
            epoch_note = f"at SINEX ref epoch {sinex['epoch'].strftime('%Y/%m/%d %H:%M')}"
        ref_precision = sinex["sigma_3d"]
        ref_label = f"SINEX/{args.station.upper()}"
        print(f"Reference : {args.sinex}")
        print(f"Station   : {args.station.upper()} ({epoch_note})")
        print(f"Ref prec  : {ref_precision*1000:.2f} mm (SINEX formal 3D σ)")
    else:
        if not args.date:
            print("FAIL: --date is required with --f5", file=sys.stderr)
            return 1
        if not os.path.isfile(args.f5):
            print(f"FAIL: F5 file not found: {args.f5}", file=sys.stderr)
            return 1
        f5 = parse_f5(args.f5, args.date)
        true_xyz = f5["xyz"]
        ref_precision = f5["sigma_3d"]
        ref_label = f"GSI-F5/{args.date}"
        print(f"Reference : {args.f5}")
        print(f"Eval date : {args.date}  (±7-day median, {f5['n_days']} days)")
        print(f"Ref coord : {f5['lat']:.8f}°N  {f5['lon']:.8f}°E  {f5['h']:.4f} m")
        print(f"Ref prec  : {ref_precision*1000:.2f} mm (68th-pctile of F5 daily scatter)")

    # ── Parse test NMEA ──────────────────────────────────────────────────────
    if not os.path.isfile(args.test):
        print(f"FAIL: test file not found: {args.test}", file=sys.stderr)
        return 1

    metric_label = "3D" if args.use_3d else "2D horizontal"
    print(f"Test      : {args.test}")
    print(f"Tolerance : {args.tolerance*100:.1f} cm  (evaluated on {metric_label} error)")
    if args.skip_epochs:
        print(f"Skip      : {args.skip_epochs} initial epochs")
    print()

    test_data = parse_nmea(args.test)
    if not test_data:
        print("FAIL: no GGA data in test file", file=sys.stderr)
        return 1

    # ── Compute metrics ──────────────────────────────────────────────────────
    m = compute_abs_metrics(true_xyz, test_data, args.skip_epochs)
    if m is None:
        print("FAIL: no usable epochs", file=sys.stderr)
        return 1

    tl, tn, th = xyz2blh(*true_xyz)
    print(f"True pos  : {tl:.8f}°N  {tn:.8f}°E  {th:.4f} m (ellipsoidal)")
    print(f"Epochs    : {m['n']}")
    print()
    print("  ENU RMS (horizontal, geoid-independent):")
    print(f"    East   : {m['rms_e']*100:8.3f} cm")
    print(f"    North  : {m['rms_n']*100:8.3f} cm")
    print()
    print("  2D horizontal error distribution:")
    print(f"    Bias   : {m['mean_2d']*100:8.3f} cm  (mean)")
    print(f"    RMS    : {m['rms_2d']*100:8.3f} cm")
    print(f"    1σ     : {m['p68_2d']*100:8.3f} cm  (68th percentile)")
    print(f"    95%    : {m['p95_2d']*100:8.3f} cm  (95th percentile)")
    print(f"    Max    : {m['max_2d']*100:8.3f} cm")
    print()
    print("  3D error distribution (Up may include geoid bias):")
    print(f"    RMS    : {m['rms_3d']*100:8.3f} cm")
    print(f"    1σ     : {m['p68_3d']*100:8.3f} cm")
    print(f"    95%    : {m['p95_3d']*100:8.3f} cm")
    print()
    print(f"  Fix rate : {m['fix_rate']:.2f}%  (GGA quality = 1 or 4)")
    print(f"  Ref prec : {ref_precision*100:.3f} cm  ({ref_label})")
    print()

    if args.plot:
        plot_results(m, ref_label)

    # ── Pass / Fail ──────────────────────────────────────────────────────────
    if args.use_3d:
        ok_1s = _criterion("1σ  (3D)", m["p68_3d"], args.tolerance, ref_precision)
        ok_95 = _criterion("95% (3D)", m["p95_3d"], args.tolerance, ref_precision)
    else:
        ok_1s = _criterion("1σ  (2D)", m["p68_2d"], args.tolerance, ref_precision)
        ok_95 = _criterion("95% (2D)", m["p95_2d"], args.tolerance, ref_precision)

    passed = ok_1s and ok_95
    print()
    print("RESULT: PASS" if passed else "RESULT: FAIL")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
