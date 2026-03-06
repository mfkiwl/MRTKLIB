# MRTKLIB v0.3.3 Release Notes

**Release date:** 2026-03-06
**Type:** Minor (benchmark tooling — no functional changes to the library)

---

## Overview

v0.3.3 introduces a **kinematic positioning benchmark** for evaluating MRTKLIB's
PPP-RTK (CLAS) and PPP (MADOCA) engines against real-world urban driving data
from the [PPC-Dataset](https://github.com/taroz/PPC-Dataset).  The dataset,
kindly released by Prof. Taro Suzuki (Chiba Institute of Technology), contains
six GNSS + IMU vehicle runs recorded in Nagoya and Tokyo, with sub-centimetre
ground truth from an Applanix POS LV 220 GNSS/INS system.

No library code, positioning algorithms, or public API changed in this release.

---

## What Changed

### Kinematic Benchmark Infrastructure (`scripts/benchmark/`)

Four new Python scripts provide an end-to-end kinematic evaluation pipeline:

| Script | Purpose |
|--------|---------|
| `cases.py` | Metadata for all 6 PPC-Dataset runs (GPS week/TOW, city/run IDs) |
| `download_l6.py` | Auto-download QZSS L6D (CLAS) and L6E (MADOCA) archive files |
| `compare_ppc.py` | Compare `rnx2rtkp` NMEA output against `reference.csv` ground truth |
| `run_benchmark.py` | Orchestrator: download → run `rnx2rtkp` → compare → summary table |

**Key features:**
- L6D archive: `https://sys.qzss.go.jp/archives/l6/` (CLAS)
- L6E archive: `https://l6msg.go.gnss.go.jp/archives/` (MADOCA, PRN auto-probe)
- Result caching: skips `rnx2rtkp` if output file is newer than all inputs
- Per-case PNG plots (`--plot`) via matplotlib
- Full CLI with `--mode clas|madoca|both`, `--case`, `--skip-epochs`, `--force`

### Benchmark Configuration (`conf/benchmark/`)

Two new configuration files tuned for the PPC-Dataset hardware:

| File | Mode | Key overrides |
|------|------|---------------|
| `clas.conf` | CLAS PPP-RTK | `ant1-anttype=*`, `pos2-isb=off`, NMEA output |
| `madoca.conf` | MADOCA PPP | `pos1-dynamics=on`, `ant1-postype=single`, NMEA output |

Receiver-specific notes:
- **Antenna type**: All PPC-Dataset rover RINEX files carry `Unknown Unknown` in
  the `ANT # / TYPE` header field → `ant1-anttype = *` wildcard is required.
- **ISB**: Septentrio mosaic-X5 has no entry in the ISB table → `pos2-isb = off`.

### Documentation (`docs/benchmark.md`)

New benchmark guide covers:
- PPC-Dataset manual download instructions
- L6 archive auto-download (`python download_l6.py`)
- Full benchmark execution walkthrough
- Complete option reference
- Metric definitions (RMS 2D/3D, fix rate, convergence time)
- Known limitations (urban multipath, no ISB, antenna calibration)

---

## Indicative Benchmark Results

Preliminary results on all six PPC-Dataset runs (GNSS-only, no IMU fusion):

| Case | Mode | N | Fix% | RMS 2D (all) | RMS 2D (fix) | Conv |
|------|------|---|------|-------------|-------------|------|
| nagoya_run1 | CLAS | 7 525 | 17% | 42.8 m | 1.24 m | 9 s |
| nagoya_run2 | CLAS | 9 390 | 30% | 305.6 m | 1.17 m | 0 s |
| nagoya_run3 | CLAS | 5 141 | 7% | 12.8 m | 0.45 m | 9 s |
| tokyo_run1  | CLAS | 11 867 | 5% | 627.1 m | 0.87 m | 15 s |
| tokyo_run2  | CLAS | 9 091 | 22% | 33.4 m | 0.59 m | 368 s |
| tokyo_run3  | CLAS | 15 241 | 7% | 41.5 m | 0.80 m | 28 s |
| nagoya_run1 | MADOCA | 1 968 | 0% | 17.4 m | — | — |
| nagoya_run2 | MADOCA | 7 494 | 0% | 39.5 m | — | — |
| nagoya_run3 | MADOCA | 2 753 | 0% | 2.65 m | — | — |
| tokyo_run1  | MADOCA | 3 084 | 0% | 1.83 m | — | — |
| tokyo_run2  | MADOCA | 8 159 | 0% | 2.89 m | — | — |
| tokyo_run3  | MADOCA | 2 795 | 0% | 0.72 m | — | — |

**Interpretation:**

- *RMS 2D (all)* includes all solution quality levels (fix + float + SPP).
  Large values reflect float/SPP epochs, which dominate in dense urban canyons.
- *RMS 2D (fix)* — Q=4 epochs only — is in the **0.5–1.2 m** range for CLAS,
  consistent with PPP-RTK performance under severe multipath.
- CLAS fix rates (5–30%) are expected at these urban locations; the dataset's
  fisheye-sky imagery confirms heavy canopy obstructions at low elevations.
- MADOCA: float-only PPP is by design in this configuration (no PPP-AR for
  kinematic). The "all" RMS reflects convergence behaviour.
- These results establish a **baseline** for future improvements (e.g., GNSS+IMU
  tight coupling, elevation masking, multipath mitigation).

> **Note:** Kinematic benchmark results are not part of the CTest regression
> suite (PPC-Dataset requires a manual download). Run manually with
> `python scripts/benchmark/run_benchmark.py`.

---

## Acknowledgements

The kinematic benchmark relies on the **PPC-Dataset** (Precise Positioning
Challenge 2024 open data):

> Taro Suzuki, Chiba Institute of Technology.
> *PPC-Dataset — GNSS/IMU driving data for precise positioning research.*
> https://github.com/taroz/PPC-Dataset

The dataset provides six urban vehicle runs (Nagoya × 3, Tokyo × 3) with
5 Hz triple-frequency multi-GNSS observations, 100 Hz IMU data, and
sub-centimetre ground truth from an Applanix POS LV 220.
We gratefully acknowledge Prof. Suzuki for making this dataset publicly available.

---

## Test Suite (unchanged at 57 tests)

No new CTest entries in this release. All 57 tests from v0.3.2 continue to pass.

| Test Suite | Tests |
|------------|-------|
| Unit tests | 12 |
| SPP regression | 2 |
| Receiver bias | 2 |
| rtkrcv real-time | 1 |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 |
| CLAS PPP-RTK (single-ch / dual-ch / ST12 / L1CL5) | 10 |
| CLAS VRS-RTK (single-ch / ST12 / dual-ch) | 9 |
| ssr2obs / ssr2osr | 3 |
| BINEX reading | 1 |
| Tier 2 absolute accuracy | 2 |
| Tier 3 position scatter | 2 |
| Fixtures (setup/cleanup/download) | 3 |
