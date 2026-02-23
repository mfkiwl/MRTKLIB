#!/bin/bash
set -euo pipefail

# Generate Test Data and Reference Position for MADOCA-PPP (madocalib)
#
# This script performs two phases:
#
#   Phase 1 — Prepare test data archive (requires convbin)
#     Cuts the full-day MIZU OBS to 1 hour using convbin, copies NAV/L6E/L6D/ATX
#     from upstream/madocalib/sample_data, and creates the tar.gz archive.
#     Skipped if the archive already exists.
#
#   Phase 2 — Generate golden reference (requires rnx2rtkp)
#     Extracts the archive and runs rnx2rtkp for all 4 test scenarios:
#       1. PPP          (sample.conf,          mdc-004 L6E)
#       2. PPP-AR       (sample_pppar.conf,    mdc-004 L6E)
#       3. PPP-AR 003   (sample_pppar.conf,    mdc-003 L6E)
#       4. PPP-AR+iono  (sample_pppar_iono.conf, mdc-004 L6E+L6D)
#
# Data: MIZU station, 2025/04/01 Session A (00:00:00–00:59:30 GPST)
#
# Prerequisites:
#   1. cmake --preset default && cmake --build build
#   2. convbin binary at project root (or on PATH)
#   3. upstream/madocalib/sample_data/data/ (for Phase 1 only)
#
# Usage:
#   bash tests/data/madocalib/generate_reference_madocalib.sh
#   bash tests/data/madocalib/generate_reference_madocalib.sh --trace     # trace level 5
#   bash tests/data/madocalib/generate_reference_madocalib.sh --trace 3   # custom level
#   bash tests/data/madocalib/generate_reference_madocalib.sh --prepare   # Phase 1 only

# Parse options
TRACE_OPTS=()
PREPARE_ONLY=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --trace)
            level="${2:-5}"
            TRACE_OPTS=(-x "$level")
            echo "Trace enabled (level $level)"
            shift; [[ $# -gt 0 && "$1" =~ ^[0-9]+$ ]] && shift
            ;;
        --prepare)
            PREPARE_ONLY=1
            shift
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# Resolve project root from script location
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
cd "$ROOT_DIR"

# Directories
DATADIR=tests/data/madocalib
UPSTREAM=upstream/madocalib/sample_data/data
ARCHIVE="${DATADIR}/madocalib_testdata.tar.gz"

# Source files (upstream)
SRC_OBS="${UPSTREAM}/rinex/MIZU00JPN_R_20250910000_01D_30S_MO.rnx"
SRC_NAV="${UPSTREAM}/rinex/BRDM00DLR_S_20250910000_01D_MN.rnx"
SRC_L6E1="${UPSTREAM}/l6_is-qzss-mdc-004/2025/091/2025091A.204.l6"
SRC_L6E2="${UPSTREAM}/l6_is-qzss-mdc-004/2025/091/2025091A.206.l6"
SRC_L6E1_003="${UPSTREAM}/l6_is-qzss-mdc-003/2025/091/2025091A.204.l6"
SRC_L6E2_003="${UPSTREAM}/l6_is-qzss-mdc-003/2025/091/2025091A.206.l6"
SRC_L6D1="${UPSTREAM}/l6_is-qzss-mdc-004/2025/091/2025091A.200.l6"
SRC_L6D2="${UPSTREAM}/l6_is-qzss-mdc-004/2025/091/2025091A.201.l6"
SRC_ATX="${UPSTREAM}/igs20.atx"

# Target files (in test data directory)
OBS="${DATADIR}/MIZU00JPN_R_20250910000_01D_30S_MO.obs"
NAV="${DATADIR}/BRDM00DLR_S_20250910000_01D_MN.rnx"
L6E1="${DATADIR}/2025091A.204.l6"
L6E2="${DATADIR}/2025091A.206.l6"
L6E1_003="${DATADIR}/2025091A.204.mdc003.l6"
L6E2_003="${DATADIR}/2025091A.206.mdc003.l6"
L6D1="${DATADIR}/2025091A.200.l6"
L6D2="${DATADIR}/2025091A.201.l6"
ATX="${DATADIR}/igs20.atx"

# Common rnx2rtkp time options
TS_TE=(-ts 2025/04/01 0:00:00 -te 2025/04/01 0:59:30 -ti 30)

# ── Phase 1: Prepare test data archive ────────────────────────────────────────

if [[ -f "$ARCHIVE" ]]; then
    echo "Archive already exists: $ARCHIVE (skipping Phase 1)"
else
    echo "=== Phase 1: Preparing test data archive ==="

    # Locate convbin
    CONVBIN="${ROOT_DIR}/convbin"
    if [[ ! -x "$CONVBIN" ]]; then
        CONVBIN="$(command -v convbin 2>/dev/null || true)"
    fi
    if [[ -z "$CONVBIN" || ! -x "$CONVBIN" ]]; then
        echo "ERROR: convbin not found." >&2
        echo "  Place the convbin binary at the project root or ensure it is on PATH." >&2
        echo "  (Future: convbin will be built as part of MRTKLIB)" >&2
        exit 1
    fi
    echo "Using convbin: $CONVBIN"

    # Verify upstream source files
    for f in "$SRC_OBS" "$SRC_NAV" \
             "$SRC_L6E1" "$SRC_L6E2" \
             "$SRC_L6E1_003" "$SRC_L6E2_003" \
             "$SRC_L6D1" "$SRC_L6D2" \
             "$SRC_ATX"; do
        if [[ ! -f "$f" ]]; then
            echo "ERROR: Upstream file not found: $f" >&2
            echo "  Ensure upstream/madocalib is populated." >&2
            exit 1
        fi
    done

    # Cut OBS to Session A (00:00:00–00:59:30) using convbin
    echo "Cutting OBS to Session A (00:00:00–00:59:30)..."
    "$CONVBIN" -r rinex -v 3.05 \
        -ts 2025/04/01 0:00:00 -te 2025/04/01 0:59:30 \
        -o "$OBS" "$SRC_OBS"

    if [[ ! -s "$OBS" ]]; then
        echo "ERROR: convbin produced empty output" >&2
        exit 1
    fi
    obs_epochs=$(grep -c "^>" "$OBS" || true)
    echo "  OBS cut: $obs_epochs epochs, $(du -sh "$OBS" | cut -f1)"

    # Copy remaining files
    echo "Copying NAV, L6E (mdc-004), L6E (mdc-003), L6D, ATX..."
    cp "$SRC_NAV"      "$NAV"
    cp "$SRC_L6E1"     "$L6E1"
    cp "$SRC_L6E2"     "$L6E2"
    cp "$SRC_L6E1_003" "$L6E1_003"
    cp "$SRC_L6E2_003" "$L6E2_003"
    cp "$SRC_L6D1"     "$L6D1"
    cp "$SRC_L6D2"     "$L6D2"
    cp "$SRC_ATX"      "$ATX"

    # Create archive
    echo "Creating archive..."
    tar -czf "$ARCHIVE" -C "$DATADIR" \
        "$(basename "$OBS")" \
        "$(basename "$NAV")" \
        "$(basename "$L6E1")" \
        "$(basename "$L6E2")" \
        "$(basename "$L6E1_003")" \
        "$(basename "$L6E2_003")" \
        "$(basename "$L6D1")" \
        "$(basename "$L6D2")" \
        "$(basename "$ATX")"

    archive_size=$(du -sh "$ARCHIVE" | cut -f1)
    echo "  Archive: $ARCHIVE ($archive_size)"

    # Clean up extracted files
    rm -f "$OBS" "$NAV" "$L6E1" "$L6E2" "$L6E1_003" "$L6E2_003" \
          "$L6D1" "$L6D2" "$ATX"

    echo "=== Phase 1 complete ==="
fi

if [[ "$PREPARE_ONLY" -eq 1 ]]; then
    echo "Done (--prepare mode)."
    exit 0
fi

# ── Phase 2: Generate golden reference ────────────────────────────────────────

echo "=== Phase 2: Generating golden reference ==="

# Locate cmake-built binary
RNX2RTKP="${ROOT_DIR}/build/rnx2rtkp"
if [[ ! -x "$RNX2RTKP" ]]; then
    echo "ERROR: $RNX2RTKP not found." >&2
    echo "  Run 'cmake --preset default && cmake --build build' first." >&2
    exit 1
fi
echo "Using binary: $RNX2RTKP"

# Extract test data
echo "Extracting data..."
tar -xzf "$ARCHIVE" -C "$DATADIR"

# Verify inputs exist
for f in "$OBS" "$NAV" "$L6E1" "$L6E2" "$L6E1_003" "$L6E2_003" \
         "$L6D1" "$L6D2"; do
    if [[ ! -f "$f" ]]; then
        echo "ERROR: Input file not found: $f" >&2
        exit 1
    fi
done

# Helper to run and verify
run_rnx2rtkp() {
    local label="$1" conf="$2" output="$3"
    shift 3
    echo "Running rnx2rtkp for ${label}..."
    "$RNX2RTKP" \
        -k "$conf" \
        "${TS_TE[@]}" \
        ${TRACE_OPTS[@]+"${TRACE_OPTS[@]}"} \
        -o "$output" \
        "$@"

    if [[ ! -s "$output" ]]; then
        echo "ERROR: Output file not generated or empty: $output" >&2
        exit 1
    fi
    local lines
    lines=$(wc -l < "$output")
    echo "  $output ($lines lines)"
}

# 1. PPP (sample.conf — armode=off, freq=l1+2, iono=dual-freq)
run_rnx2rtkp "MADOCA-PPP" \
    conf/madocalib/rnx2rtkp.conf \
    "${DATADIR}/madocalib_MIZU_20250401-0000.pp.pos" \
    "$OBS" "$NAV" "$L6E1" "$L6E2"

# 2. PPP-AR mdc-004 (sample_pppar.conf — armode=continuous, freq=l1+2+3+4)
run_rnx2rtkp "MADOCA PPP-AR (mdc-004)" \
    conf/madocalib/rnx2rtkp_pppar.conf \
    "${DATADIR}/madocalib_MIZU_20250401-0000.pppar.pos" \
    "$OBS" "$NAV" "$L6E1" "$L6E2"

# 3. PPP-AR mdc-003 (same config, different L6E data)
run_rnx2rtkp "MADOCA PPP-AR (mdc-003)" \
    conf/madocalib/rnx2rtkp_pppar.conf \
    "${DATADIR}/madocalib_MIZU_20250401-0000.pppar_003.pos" \
    "$OBS" "$NAV" "$L6E1_003" "$L6E2_003"

# 4. PPP-AR + ionosphere correction (L6E + L6D)
run_rnx2rtkp "MADOCA PPP-AR+iono" \
    conf/madocalib/rnx2rtkp_pppar_iono.conf \
    "${DATADIR}/madocalib_MIZU_20250401-0000.pppar_ion.pos" \
    "$OBS" "$NAV" "$L6E1" "$L6E2" "$L6D1" "$L6D2"

# Clean up extracted files
rm -f "$OBS" "$NAV" "$L6E1" "$L6E2" "$L6E1_003" "$L6E2_003" \
      "$L6D1" "$L6D2" "$ATX"

echo "=== Phase 2 complete ==="
echo "Reference data generated successfully."
