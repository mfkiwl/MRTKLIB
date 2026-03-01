# Changelog

All notable changes to MRTKLIB are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
- **MADOCALIB**: [QZSS-Strategy-Office/madocalib](https://github.com/QZSS-Strategy-Office/madocalib) ver.2.0 (`8091004`)

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

[v0.2.0]: https://github.com/h-shiono/MRTKLIB/compare/v0.1.0...v0.2.0
[v0.1.0]: https://github.com/h-shiono/MRTKLIB/releases/tag/v0.1.0
