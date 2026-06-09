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

# ── Optional: pin CPU frequency on cores 3-4 to avoid turbo variance ─────────
# Requires cpupower or direct sysfs access; skip silently if unavailable.

freq_pinned=1
if command -v cpupower &>/dev/null && [[ $EUID -eq 0 ]]; then
  echo "==> Pinning CPU 3-4 to performance governor..."
  cpupower -c 3-4 frequency-set -g performance &>/dev/null && freq_pinned=1
elif [[ -w /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor ]]; then
  for cpu in 3 4; do
    echo performance > /sys/devices/system/cpu/cpu${cpu}/cpufreq/scaling_governor
  done
  freq_pinned=1
fi

restore_freq() {
  if [[ $freq_pinned -eq 1 ]]; then
    if command -v cpupower &>/dev/null && [[ $EUID -eq 0 ]]; then
      cpupower -c 3-4 frequency-set -g schedutil &>/dev/null || true
    elif [[ -w /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor ]]; then
      for cpu in 3 4; do
        echo schedutil > /sys/devices/system/cpu/cpu${cpu}/cpufreq/scaling_governor || true
      done
    fi
  fi
}
trap restore_freq EXIT

# ── Run benchmark pinned to isolated cores ────────────────────────────────────

echo "==> Running benchmark on CPU(s) $CPUS..."
echo ""

exec taskset -c "$CPUS" "$BENCH_BIN" "$@"
