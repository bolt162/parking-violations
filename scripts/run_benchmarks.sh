#!/usr/bin/env bash
# ============================================================================
#  run_benchmarks.sh — Run Phase 1 benchmarks (serial baseline)
#
#  Usage:
#    ./scripts/run_benchmarks.sh <path_to_merged_csv> [iterations]
#
#  Example:
#    ./scripts/run_benchmarks.sh ../Parking_Violations_Data/parking_violations_merged.csv 12
#
#  Outputs:
#    results/load_benchmark.csv
#    results/search_linear.csv
#    results/search_indexed.csv
# ============================================================================

set -euo pipefail

CSV_FILE="${1:?Usage: $0 <csv_file> [iterations]}"
ITERATIONS="${2:-12}"

# Resolve paths
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
RESULTS_DIR="$PROJECT_DIR/results"

# Check build exists
if [ ! -f "$BUILD_DIR/src/load_benchmark" ]; then
    echo "ERROR: Build not found. Run:"
    echo "  cd $BUILD_DIR && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j"
    exit 1
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

echo "============================================"
echo "  Phase 1 Serial Benchmarks"
echo "  CSV: $CSV_FILE"
echo "  Iterations: $ITERATIONS"
echo "  Results: $RESULTS_DIR/"
echo "============================================"
echo ""

# ── A: Load benchmark ─────────────────────────────────────────────────────
echo "=== A: Load Benchmark ==="
"$BUILD_DIR/src/load_benchmark" "$CSV_FILE" "$ITERATIONS" \
    "$RESULTS_DIR/load_benchmark.csv" \
    2>&1 | tee "$RESULTS_DIR/load_benchmark.log"
echo ""

# ── B: Search benchmark (LinearSearch) ────────────────────────────────────
echo "=== B: Search Benchmark (LinearSearch) ==="
"$BUILD_DIR/src/search_benchmark" "$CSV_FILE" "$ITERATIONS" \
    "$RESULTS_DIR/search_linear.csv" \
    2>&1 | tee "$RESULTS_DIR/search_linear.log"
echo ""

# ── C: Search benchmark (IndexedSearch) ───────────────────────────────────
echo "=== C: Search Benchmark (IndexedSearch) ==="
"$BUILD_DIR/src/search_benchmark" "$CSV_FILE" "$ITERATIONS" \
    "$RESULTS_DIR/search_indexed.csv" --indexed \
    2>&1 | tee "$RESULTS_DIR/search_indexed.log"
echo ""

echo "============================================"
echo "  All benchmarks complete!"
echo "  Results in: $RESULTS_DIR/"
echo "============================================"
echo ""
echo "To generate plots:"
echo "  python3 $SCRIPT_DIR/plot_results.py $RESULTS_DIR"
