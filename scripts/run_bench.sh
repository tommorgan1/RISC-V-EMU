#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$REPO_ROOT/32b/build"
BENCH_BIN="$BUILD_DIR/bench/riscv-bench"

# Cores to pin to (isolated via isolcpus= in kernel cmdline)
CPUS="3-4"

# ── Build if binary is missing or source is newer ────────────────────────────

needs_build=0
if [[ ! -x "$BENCH_BIN" ]]; then
  needs_build=1
else
  # rebuild if any source file is newer than the binary
  if find "$REPO_ROOT/32b/src" "$REPO_ROOT/32b/inc" "$REPO_ROOT/32b/bench" \
       -name "*.cpp" -o -name "*.hpp" -o -name "*.S" -o -name "*.ld" 2>/dev/null \
     | xargs -r stat --format="%Y %n" \
     | sort -rn \
     | head -1 \
     | awk -v bin="$(stat --format="%Y" "$BENCH_BIN")" '{exit ($1 > bin) ? 0 : 1}'; then
    needs_build=1
  fi
fi

if [[ $needs_build -eq 1 ]]; then
  echo "==> Building benchmark..."
  cmake --build "$BUILD_DIR" --target riscv-bench bench_elf -j"$(nproc)"
fi

# ── Pin CPU frequency on cores to avoid idle-downclocking variance ───────────
# Requires root. Sets the performance governor so the CPU doesn't drop to an
# idle P-state between nanobench iterations. Turbo stays enabled so the warmup
# epochs ramp the core to full boost before measurement begins.
# (Disabling turbo via no_turbo would lock to the 1.4 GHz base P-state, ~3x
# slower than boost — use that only if you need stable numbers under sustained
# thermal load.)

freq_pinned=0

if [[ $EUID -ne 0 ]]; then
  echo "WARNING: not running as root — CPU governor will not be set."
  echo "         Run with sudo to prevent idle downclocking between iterations."
  echo ""
elif command -v cpupower &>/dev/null; then
  cpupower -c "$CPUS" frequency-set -g performance &>/dev/null
  freq_pinned=1
  echo "==> CPU $CPUS governor → performance (turbo on)"
  echo ""
fi

restore_freq() {
  [[ $freq_pinned -eq 1 ]] || return
  cpupower -c "$CPUS" frequency-set -g powersave &>/dev/null || true
}
trap restore_freq EXIT

# ── Run benchmark pinned to isolated cores ────────────────────────────────────

PERF_DATA="$REPO_ROOT/perf.data"

echo "==> Running benchmark on CPU(s) $CPUS..."
echo ""

perf record -q -o "$PERF_DATA" taskset -c "$CPUS" "$BENCH_BIN" "$@"
bench_exit=$?

echo ""
echo "==> perf report (top 50 symbols):"
perf report --stdio --no-children -i "$PERF_DATA" 2>/dev/null | head -50

exit $bench_exit
