#!/usr/bin/env bash
# dump_traces.sh — compile and dump Yk HIR traces for every AWFY benchmark.
#
# Builds the yk-location-method-entry branch binary if needed, then runs each
# benchmark with YKD_LOG_IR so every compiled trace is written to traces/.
#
# Usage: bash dump_traces.sh [output_dir]
#   output_dir defaults to ./traces

set -euo pipefail

OUTDIR="${1:-traces}"
BUILD_DIR="cmake-yk-debug"
BINARY="${BUILD_DIR}/SOM++"

AWFY="core-lib/Examples/AreWeFastYet"
CP="${AWFY}:${AWFY}/Core:core-lib/Smalltalk:${AWFY}/Richards:${AWFY}/NBody:${AWFY}/CD:${AWFY}/DeltaBlue:${AWFY}/Havlak:${AWFY}/Json"
HARNESS="${AWFY}/Harness.som"

rm -fr $OUTDIR
just yk_config=/home/pd/yk/bin/yk-config build-yk-debug

# Enough iterations to warm up the JIT and see compiled traces for each benchmark.
declare -A ITERS=(
  [Richards]=100
  [Bounce]=500
  [List]=500
  [Permute]=500
  [Queens]=500
  [Towers]=300
  [Storage]=500
  [Sieve]=1000
  [NBody]=250000
  [Mandelbrot]=500
  [DeltaBlue]=500
  [Json]=100
)


mkdir -p "${OUTDIR}"

echo "Binary:     ${BINARY}"
echo "Output dir: ${OUTDIR}"
echo ""

for bench in "${!ITERS[@]}"; do
  iters="${ITERS[$bench]}"
  hirfile="${OUTDIR}/${bench}.hir"
  dbgfile="${OUTDIR}/${bench}.debugstrs"
  logfile="${OUTDIR}/${bench}.log"
  printf "  %-12s %5d iters -> %s.*\n" "${bench}" "${iters}" "${OUTDIR}/${bench}"

  # hir,debugstrs: HIR instructions + source-location comments per trace.
  # Serialise compilation so all output is flushed before the next run.
  YKD_LOG_IR="${hirfile}:hir,debugstrs" \
    YKD_LOG="${logfile}:3" \
    YKD_SERIALISE_COMPILATION=1 \
    "${BINARY}" -cp "${CP}" "${HARNESS}" "${bench}" 1 "${iters}" \
    2>>"${logfile}" || echo "    [WARN] ${bench} exited non-zero (see ${logfile})"

  # debugstrs are comment lines (^; ) embedded in the hir file; extract separately.
  grep "^; " "${hirfile}" >"${dbgfile}" 2>/dev/null || true

  if [[ -s "${hirfile}" ]]; then
    traces=$(grep -c "^--- Begin hir ---" "${hirfile}" || true)
    printf "    %d trace(s) | hir %d lines | debugstrs %d lines | log %d lines\n" \
      "${traces}" "$(wc -l <"${hirfile}")" \
      "$(wc -l <"${dbgfile}")" \
      "$(wc -l <"${logfile}")"
  else
    echo "    [WARN] no HIR output written"
  fi
done

echo ""
echo "Done. Traces in ${OUTDIR}/"
ls -lh "${OUTDIR}"/*.hir 2>/dev/null || true
