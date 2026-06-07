#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="32b/build"
SOURCE_DIR="32b"
JOBS=$(nproc 2>/dev/null || echo 4)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${REPO_ROOT}"

echo "=== RISC-V Emulator Build & Test ==="

# ---- Submodules -----------------------------------------------------------
echo "[1/4] Initialising submodules..."
git submodule update --init --recursive

# Check riscv-tests explicitly
if [ ! -d "external/riscv-tests/isa/rv32ui" ]; then
    echo "ERROR: riscv-tests submodule not populated at external/riscv-tests"
    echo "Check your .gitmodules contains an entry for external/riscv-tests"
    echo "If not yet added, run:"
    echo "  git submodule add https://github.com/riscv-software-src/riscv-tests.git external/riscv-tests"
    exit 1
fi

# ---- CMake Configure ------------------------------------------------------
echo "[2/4] Configuring with CMake..."
cmake -S "${SOURCE_DIR}" \
      -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE=Release

# ---- Build ----------------------------------------------------------------
echo "[3/4] Building (jobs: ${JOBS})..."
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

# ---- Build compliance ELFs ------------------------------------------------
echo "[3b/4] Building compliance ELFs..."
if cmake --build "${BUILD_DIR}" --target rv32ui_elfs --parallel "${JOBS}"; then
    echo "[4/4] Running ALL tests (unit + compliance)..."
    ctest --test-dir "${BUILD_DIR}" \
          --output-on-failure \
          --parallel "${JOBS}" \
          --timeout 30
else
    echo "WARN: rv32ui_elfs target unavailable — running unit tests only..."
    ctest --test-dir "${BUILD_DIR}" \
          --output-on-failure \
          --parallel "${JOBS}" \
          --timeout 30 \
          --exclude-regex "compliance/"
fi

echo ""
echo "=== Done ==="