# Changelog

All notable changes to MRTKLIB are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v0.3.2] - 2026-03-06

Patch release — three-tier test methodology with absolute accuracy and position
scatter checks. No functional changes to the library.

### Added

- **Three-tier test methodology** ([docs/test-accuracy-methodology.md](docs/test-accuracy-methodology.md))
  — Formalises Tier 1 (relative porting correctness), Tier 2 (absolute geodetic
  accuracy vs SINEX / GSI F5), and Tier 3 (position scatter / precision) layers.
- **Tier 2 absolute accuracy tests** (2 new CTest entries):
  - `madocalib_pppar_abs_check` — MADOCA PPP-AR vs IGS SINEX MIZU (GPS week 2383), 2D horizontal ≤ 0.100 m
  - `claslib_ppp_rtk_2ch_abs_check` — CLAS 2CH vs GSI F5 TSUKUBA3 2025/06/06, 2D horizontal ≤ 0.300 m
- **Tier 3 position scatter tests** (2 new CTest entries):
  - `madocalib_ppp_scatter` — MADOCA-PPP 2D scatter ≤ 0.150 m (1σ=7.4 cm, 95%=11.2 cm, skip=30)
  - `claslib_ppp_rtk_2ch_scatter` — CLAS 2CH scatter ≤ 0.400 m (1σ=5.8 cm, 95%=25.2 cm, skip=20)
- **New comparison scripts**: `compare_pos_abs.py`, `compare_nmea_abs.py` (Tier 2);
  `check_pos_scatter.py` (Tier 3).
- **`scripts/tests/_geo.py`** — Shared geodetic utilities (`blh2xyz`, `xyz2blh`,
  `xyz2enu`, `nmea_to_deg`) replacing 4× inline duplication across test scripts.
- **`ruff.toml`** at project root — Ruff linting for all Python scripts
  (`E`, `F`, `W`, `I`, `D` rules, Google docstring convention).
- **`.vscode/settings.json`** — Ruff format-on-save and `scripts/.venv` interpreter.

### Changed

- `scripts/pyproject.toml` — Reduced to package metadata only; ruff config moved
  to root `ruff.toml`.
- Three debug plot scripts moved from `scripts/tests/` to `scripts/plotting/`
  (`plot_gloeph.py`, `plot_igp.py`, `plot_peph.py`).

### Removed

- `scripts/fix_trace_fwd_decls.py`, `scripts/migrate_trace_ctx.py` — One-time
  migration tools no longer needed.
- `scripts/plotting/plot_lex_{eph,ion,ure}.py` — LEX service retired 2020.

### Fixed

- **NMEA ellipsoidal height** — `compare_nmea_abs.py` now correctly computes
  `h_ell = MSL + geoid_separation` from GGA fields [9] and [11]; previously
  field[11] was not summed, causing ~40 m Up errors.
- **Absolute check 2D mode** — `--use-2d` flag in `compare_pos_abs.py` now
  consistently governs both the reported metric and the pass/fail criterion.

### Test Results

All 57 tests pass (53 carried from v0.3.1 + 4 new):

| Test Suite | Tests |
|------------|-------|
| Unit tests | 12 |
| SPP / receiver bias / rtkrcv | 5 |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 |
| CLAS PPP-RTK + VRS-RTK | 19 |
| ssr2obs / ssr2osr / BINEX | 4 |
| Tier 2 absolute accuracy | 2 ← new |
| Tier 3 position scatter | 2 ← new |
| Fixtures | 3 |

---

## [v0.3.1] - 2026-03-06

Patch release — MADOCALIB PPP-AR regression test quality improvement.
No functional changes.

### Changed

- **MADOCALIB PPP-AR reference data** — Replaced `pppar` and `pppar_ion`
  reference `.pos` files with outputs from upstream MADOCALIB built with
  `-DLAPACK` (Accelerate framework). The previous LU-solver reference caused
  artificial 1.5–3.8 cm offsets unrelated to porting correctness.
- **Test tolerances tightened** (when `LAPACK_FOUND` is true):
  - `madocalib_pppar_check`: 0.020 m → **0.008 m**
  - `madocalib_pppar_ion_check`: 0.040 m → **0.005 m**
  - Fallback to original thresholds when LAPACK is unavailable (internal LU vs LAPACK reference diverges ~1.5–3.8 cm)

### Background

Investigation confirmed that upstream MADOCALIB already supports LAPACK via
`#ifdef LAPACK` in `rtkcmn.c`. Building upstream with `-DLAPACK -framework
Accelerate` reveals the true implementation difference: MRTKLIB vs upstream
LAPACK is **0.41 cm** (pppar) and **0.25 cm** (pppar_ion), confirming the
porting is correct. The 1.5–3.8 cm difference seen in v0.3.0 was entirely
attributable to LU vs LAPACK numerical divergence within the upstream itself.

### Test Results

All 53 tests pass. Tolerances when built with LAPACK (tight) vs without (wide):

| Test | 3D RMS | Tolerance (LAPACK) | Tolerance (no LAPACK) |
|------|--------|-------------------|----------------------|
| madocalib_pppar | 0.41 cm | 0.008 m (~50% margin) | 0.020 m |
| madocalib_pppar_ion | 0.25 cm | 0.005 m (~50% margin) | 0.040 m |

---

## [v0.3.0] - 2026-03-06

CLASLIB integration — adds QZSS CLAS L6D augmentation (PPP-RTK and VRS-RTK)
via [CLASLIB](https://github.com/QZSS-Strategy-Office/claslib) ver.0.8.2 (`9e714b9`).
Includes the complete CSSR decoder, grid correction engine, PPP-RTK positioning,
VRS double-differenced RTK, dual-channel L6 support, and SSR→OSR utilities.

### Added

- **CLAS CSSR decoder** (`mrtk_clas.c`) — Full ST1–ST12 Compact SSR message
  decoder supporting IS-QZSS-L6-007 two-channel L6 input.
- **CLAS grid correction engine** (`mrtk_clas_grid.c`) — Tropospheric and STEC
  grid interpolation derived from GSILIB v1.0.3 algorithms.
- **PPP-RTK positioning engine** (`mrtk_ppp_rtk.c`) — CLAS centimeter-level
  augmentation using double-differenced carrier-phase OSR corrections.
  Accuracy: **5.9 cm 3D RMS, 99.86% fix** (single-channel, DOY 239/2019).
- **VRS DD-RTK positioning engine** (`mrtk_vrs.c`, ~2050 lines) — Virtual
  Reference Station double-differenced RTK using CLAS OSR corrections.
  Accuracy: **3.3 cm 3D RMS, 99.86% fix** (DOY 239/2019).
- **Multi-L6E dual-channel support** — `nav->ssr_ch[SSR_CH_NUM][MAXSAT]` and
  `_mcssr[SSR_CH_NUM]` decoder array support two independent L6 channels
  (ST12 mode, IS-QZSS-L6-007). ST12 accuracy: **10.8 cm 3D RMS, 98.75% fix**.
- **BINEX file reading** — `readbnxt()` enables post-processing from Septentrio
  BINEX raw observation files.
- **Per-system frequency selection** (`posopt[10–12]`) — Independent L1/L2/L5
  signal selection for GPS, GLONASS, Galileo, and QZSS.
- **`ssr2obs` utility** — Standalone tool converting CLAS L6 corrections to
  RINEX3 VRS pseudo-observations; also outputs OSR CSV and RTCM3 MSM4 format.
- **`ssr2osr` utility** — Post-processing OSR verification tool (SSR→OSR via
  `postpos()` with `PMODE_SSR2OSR`).
- **`dumpcssr` utility** — CSSR message dump tool for L6 stream inspection.
- **`clas_ssr2osr()` wrapper** — Public API for SSR→OSR conversion callable
  from within `rtkpos()`.
- **23 new regression tests** — PPP-RTK (10), VRS-RTK (9), ssr2obs/ssr2osr (3),
  BINEX (1); total test count raised from 30 to 53.
- **NMEA comparison infrastructure** (`scripts/tests/compare_nmea.py`) —
  GGA-based accuracy comparison for VRS-RTK regression testing.

### Changed

- **`rtkpos()` dispatch** — Added `PMODE_PPP_RTK`, `PMODE_VRS_RTK`, and
  `PMODE_SSR2OSR` branches; CLAS context initialised in `postpos()`.
- **`prcopt_t`** — Extended `err[5]→err[8]` (CLAS measurement noise model) and
  `posopt[6]→posopt[13]` (per-system frequency and PPP-RTK knobs).
- **`sat_lambda()` in PPP-RTK** — Switched from obsdef table lookup to direct
  obs-code→frequency mapping, fixing QZS L2 wavelength (0 → correct value)
  and raising fix rate from 97% → 99.86%.
- **`clas_osr_zdres()`** — When `y==NULL` (VRS mode), writes OSR directly to
  the caller's buffer without an intermediate `osrtmp` redirect.
- **`tidedisp()` call** — Corrected bitmask routing (1=solid, 2=OTL, 4=pole)
  for grid OTL displacement.
- **Unified copyright headers** — All source files (lib + apps, ~112 files)
  updated with full 9-party contributor lineage, SPDX identifier, and
  Doxygen `@file` blocks separated from license comments.
- **`LICENSE.txt`** — Added Mitsubishi Electric Corp. (2015–) and Geospatial
  Information Authority of Japan (2014–) copyright lines.
- **`README.md`** — CLASLIB marked Integrated (was Planned); License &
  Attributions table added.

### Fixed

- **`set_ssr_ch_idx()` / `set_mcssr_ch()`** — Added `[0, SSR_CH_NUM)` bounds
  guard to prevent out-of-bounds access on `nav->ssr_ch` and `_mcssr` arrays.
- **`memset` sizeof targets** — `biaosb->spb` and `biaosb->vspb` now use
  `sizeof` of their own member (previously used sibling member's size).
- **SIS/IODE boundary detection** — `clas_osr_satcorr_update()` uses
  `prev_orb_tow` to detect orbit-epoch advancement; removed incorrect
  `orb_tow == clk_tow` requirement that broke ST12 (orbit always 5 s ahead
  of clock in ST12 streams).
- **`clas_mode` gate in `postpos()`** — `PMODE_SSR2OSR` included in CLAS
  context initialisation check.
- **`compensatedisp()` SSR crash** — Fixed null pointer dereference when SSR
  corrections are not yet available.

### Known Limitations

- **Real-time CLAS**: `rtkrcv` supports a single CLAS stream (channel 0).
  Real-time dual-L6 channel switching is not yet implemented.
- **Real-time L6D**: Ionospheric STEC correction (L6D) is not available in
  real-time mode. PPP-AR+iono requires post-processing (`rnx2rtkp`).
- **Static mode**: PPP-RTK in static receiver mode is not yet validated
  (test data unavailable).

### Test Results

All 53 tests pass:

| Test Suite | Tests | Key Accuracy |
|------------|-------|-------------|
| Unit tests | 12 | — |
| SPP regression | 2 | — |
| Receiver bias | 2 | — |
| rtkrcv real-time | 1 | — |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 | <0.5 cm / ~1.6 cm / ~3.8 cm 3D RMS |
| CLAS PPP-RTK (single-ch / dual-ch / ST12 / L1CL5) | 10 | 5.9 cm / — / 10.8 cm / — |
| CLAS VRS-RTK (single-ch / ST12 / dual-ch) | 9 | 3.3 cm / — / — |
| ssr2obs / ssr2osr | 3 | — |
| BINEX reading | 1 | — |
| Fixtures (setup/cleanup/download) | 3 | — |

### Upstream References

- **CLASLIB**: [claslib](https://github.com/QZSS-Strategy-Office/claslib) ver.0.8.2 (`9e714b9`)

---

## [v0.2.0] - 2026-03-02

MADOCALIB PPP engine integration — replaces the MALIB PPP engine with
[MADOCALIB](https://github.com/QZSS-Strategy-Office/madocalib) ver.2.0 (`8091004`)
for higher-accuracy PPP/PPP-AR processing with L6E and L6D correction support.

### Added

- **MADOCALIB PPP/PPP-AR engine** — Full integration of the MADOCALIB positioning
  algorithms (PPP, PPP-AR, PPP-AR+iono) from upstream ver.2.0, replacing the
  previous MALIB PPP stub.
- **L6E SSR decoder** (`mrtk_madoca.c`) — QZSS L6E orbit/clock/bias correction
  stream decoder (MCSSR format).
- **L6D ionospheric decoder** (`mrtk_madoca_iono.c`) — QZSS L6D wide-area STEC
  ionospheric correction decoder for PPP-AR+iono post-processing.
- **Real-time PPP** — `rtkrcv` now runs MADOCALIB PPP engine with L6E corrections
  via SBF file stream replay; verified with `rtkrcv_rt` regression test.
- **MADOCALIB regression tests** (8 tests) — PPP, PPP-AR (mdc-004/mdc-003),
  PPP-AR+iono accuracy checks against upstream reference data (MIZU station).
- **Python-based `.pos` comparison tool** (`tests/cmake/compare_pos.py`) —
  Configurable tolerance comparison for regression testing.
- **Unified copyright headers** — All 104 source files updated with full
  7-party contributor lineage and SPDX-License-Identifier.

### Changed

- **PPP engine** — `pppos()` in `mrtk_ppp.c` is now the MADOCALIB implementation
  (previously MALIB). Includes PPP-AR (`mrtk_ppp_ar.c`) and ionospheric
  correction (`mrtk_ppp_iono.c`) modules.
- **Signal selection** — Unified to obsdef tables aligned with upstream MADOCALIB
  (`code2idx()` frequency index mapping).
- **SPP ISB model** — Aligned with upstream MADOCALIB observation selection.
- **BLAS guard** — Added zero-dimension guard in `matmul()` to prevent BLAS
  `dgemm` LDC=0 errors when filter state count is zero.
- **`prev_qr[]` state management** — Zero-initialized in `rtkinit()` and updated
  after `rtkpos()` in both `procpos()` and `rtksvrthread()` to ensure correct
  `const_iono_corr()` convergence detection.
- **File naming** — Renamed for consistency with `mrtk_madoca_*` convention:
  - `mrtk_rtcm3lcl.c` → `mrtk_rtcm3_local_corr.c`
  - `mrtk_mdciono.c` → `mrtk_madoca_iono.c`
- **CI workflow** — Updated accuracy analysis to use MADOCALIB PPP output
  (`out_madocalib_ppp.pos`) with MIZU station reference coordinates.
- **`prn[6]` documentation** — Corrected field comment from `dcb` to `ifb`
  (Inter-Frequency Bias); `stats-prndcb` retained as legacy alias.
- **`prcopt_t` cleanup** — Removed unused `ppp_engine` field and related
  constants/configuration (`pos1-ppp-engine`).

### Removed

- **`mrtk_ppp_madoca.c`** — Dead stub file (SOLQ_NONE output only); the actual
  MADOCALIB engine is integrated directly into `mrtk_ppp.c`.
- **`MRTK_PPP_ENGINE_*` constants** — Engine selection mechanism removed as
  MADOCALIB is now the sole PPP engine.

### Known Limitations

- **Real-time L6D**: Ionospheric STEC correction input is not available in
  real-time mode (`rtkrcv`). PPP-AR+iono requires post-processing (`rnx2rtkp`).
- **Single correction stream**: `rtkrcv` supports one correction input
  (`inpstr3`). Multiple L6E channels require receiver-side multiplexing.
- **LAPACK numerical difference**: MRTKLIB uses system LAPACK while upstream
  MADOCALIB uses an embedded LU solver, causing ~1.5–3.8 cm 3D RMS differences
  in PPP-AR solutions. Test tolerances are adjusted accordingly.

### Test Results

All 30 tests pass (12 unit + 18 regression):

| Test Suite | Tests | Status |
|------------|-------|--------|
| Unit tests | 12 | PASS |
| SPP regression | 2 | PASS |
| Receiver bias | 2 | PASS |
| rtkrcv real-time | 1 | PASS |
| MADOCA PPP | 2 | PASS |
| MADOCA PPP-AR (mdc-004) | 2 | PASS |
| MADOCA PPP-AR (mdc-003) | 2 | PASS |
| MADOCA PPP-AR+iono | 2 | PASS |
| Fixtures (setup/cleanup) | 5 | PASS |

### Upstream References

- **MALIB**: [JAXA-SNU/MALIB](https://github.com/JAXA-SNU/MALIB) feature/1.2.0 (`f006a34`)
- **MADOCALIB**: [madocalib](https://github.com/QZSS-Strategy-Office/madocalib) ver.2.0 (`8091004`)

## [v0.1.0] - 2026-02-23

Initial release — MALIB structural migration complete.

### Added

- **MRTKLIB core architecture** — Thread-safe `mrtk_ctx_t` context, POSIX/C11
  pure implementation, CMake build system.
- **Domain-driven directory structure** — `src/core/`, `src/pos/`, `src/data/`,
  `src/rtcm/`, `src/models/`, `src/stream/`, `src/madoca/`.
- **Applications** — `rnx2rtkp` (post-processing), `rtkrcv` (real-time),
  `recvbias` (receiver bias estimation).
- **Regression test framework** — CTest-based SPP, receiver bias, and real-time
  PPP tests with reference data comparison.
- **MALIB integration** — Structural base from JAXA MALIB feature/1.2.0
  (directory layout, threading, stream I/O).

[v0.3.2]: https://github.com/h-shiono/MRTKLIB/compare/v0.3.1...v0.3.2
[v0.3.1]: https://github.com/h-shiono/MRTKLIB/compare/v0.3.0...v0.3.1
[v0.3.0]: https://github.com/h-shiono/MRTKLIB/compare/v0.2.0...v0.3.0
[v0.2.0]: https://github.com/h-shiono/MRTKLIB/compare/v0.1.0...v0.2.0
[v0.1.0]: https://github.com/h-shiono/MRTKLIB/releases/tag/v0.1.0
