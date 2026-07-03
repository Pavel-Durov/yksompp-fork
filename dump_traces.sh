#!/usr/bin/env bash
# dump_traces.sh — compile and dump Yk HIR traces for every AWFY benchmark,
# plus capture release YKD_LOG_STATS for each.
#
# Each benchmark gets its own directory ${OUTDIR}/<bench>/ containing:
#   debug.hir/.debugstrs/.log/.ykstats   DEBUG trace dump (HIR + source locs)
#   debug.dump                           bytecode disassembly (-d)
#   release.ykstats/.log                 RELEASE JIT coverage (source of truth)
#   heatmap.bytecodes.tsv                hit count per File.som:line:BYTECODE
#   heatmap.lines.tsv                    hit count per File.som:line
#   anchors.tsv                          where root/side traces start (hot anchors)
#   som/<File>.som                       copy of each source file seen in the traces
#   som/<File>.som.heat                  that source annotated with per-line hit counts
#
# Two passes per benchmark:
#   1. DEBUG build — YKD_LOG_IR dumps every compiled trace's HIR with source-
#      location debugstrs; the heatmap and the .som copies are derived from these.
#   2. RELEASE build — YKD_LOG_STATS captures real JIT coverage. Release emits far
#      fewer LLVM basic blocks per bytecode than debug, so it is the source of
#      truth for trace counts and "Trace too long" aborts; debug overstates them.
#
# Usage: bash dump_traces.sh [bench ...]
#   With no bench args, dumps every known benchmark; otherwise only the named
#   ones, in the order given. Names not in the ITERS map below get DEFAULT_ITERS
#   warm-up iterations.
#   OUTDIR=dir    output directory (default ./traces)
#   REL_MULT=N    run release with N× each benchmark's iteration count (default 1)
#   DEFAULT_ITERS=N  warm-up iters for benchmarks not in the ITERS map (default 100)
#   SKIP_BUILD=1  reuse existing debug/release builds (skip the two `just` builds)
#   HEATMAP=0     skip the non-yk bytecode-execution heatmap (default 1 = enabled)
# Examples:
#   bash dump_traces.sh                       # all benchmarks
#   bash dump_traces.sh Richards Havlak CD    # just these three
#   OUTDIR=traces-rich SKIP_BUILD=1 bash dump_traces.sh Richards

set -euo pipefail

OUTDIR="${OUTDIR:-traces}"
BUILD_DIR="cmake-yk-debug"
BINARY="${BUILD_DIR}/SOM++"
REL_BUILD_DIR="cmake-yk-release"
REL_BINARY="${REL_BUILD_DIR}/SOM++"
REL_MULT="${REL_MULT:-1}"
DEFAULT_ITERS="${DEFAULT_ITERS:-100}"
# Bytecode execution heatmap: a NON-yk (pure interpreter) binary with per-bytecode
# counters annotates each benchmark's -d disassembly with "[hits: N]". Must be
# non-yk: under the JIT the hot bytecodes run in compiled traces and bypass the
# interpreter's counter, which would invert the heatmap.
HEATMAP="${HEATMAP:-1}"
HEATMAP_BINARY="cmake-heatmap/SOM++"
# The heatmap pass runs on the (slower) pure interpreter; relative hotness is
# stable well before a benchmark's full iteration count, so cap it here.
HEATMAP_ITERS="${HEATMAP_ITERS:-500}"

# Extract an integer field from a YKD_LOG_STATS JSON file (empty if absent).
jstat() { grep -oE "\"$2\"[[:space:]]*:[[:space:]]*[0-9]+" "$1" 2>/dev/null | grep -oE '[0-9]+$' || true; }

AWFY="core-lib/Examples/AreWeFastYet"
CP="${AWFY}:${AWFY}/Core:core-lib/Smalltalk:${AWFY}/Richards:${AWFY}/NBody:${AWFY}/CD:${AWFY}/DeltaBlue:${AWFY}/Havlak:${AWFY}/Json"
HARNESS="${AWFY}/Harness.som"

if [[ "${SKIP_BUILD:-0}" == 1 ]]; then
  echo "SKIP_BUILD=1: reusing existing ${BINARY} and ${REL_BINARY}"
else
  just yk_config=/home/pd/yk/bin/yk-config build-yk-debug
  just yk_config=/home/pd/yk/bin/yk-config build-yk-release
  if [[ "${HEATMAP}" == 1 ]]; then
    just build-heatmap
  fi
fi

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
  [CD]=250
  [Havlak]=250
  [Json]=100
)


# Benchmarks to process: positional args in the order given, or all known ones.
if (( $# > 0 )); then
  BENCHES=( "$@" )
else
  BENCHES=( "${!ITERS[@]}" )
fi

# Each benchmark's own subdirectory is wiped and rebuilt in the loop below, so a
# rerun is clean for the benchmarks it processes and leaves the others alone.
mkdir -p "${OUTDIR}"

echo "Debug binary:   ${BINARY}"
echo "Release binary: ${REL_BINARY}  (${REL_MULT}x iters)"
echo "Output dir:     ${OUTDIR}"
echo "Benchmarks:     ${BENCHES[*]}"
echo ""

for bench in "${BENCHES[@]}"; do
  iters="${ITERS[$bench]:-$DEFAULT_ITERS}"
  if [[ -z "${ITERS[$bench]:-}" ]]; then
    echo "  [note] ${bench} not in ITERS map; using DEFAULT_ITERS=${DEFAULT_ITERS}"
  fi
  bdir="${OUTDIR}/${bench}"
  rm -rf "${bdir}"
  mkdir -p "${bdir}/som"
  hirfile="${bdir}/debug.hir"
  dbgfile="${bdir}/debug.debugstrs"
  logfile="${bdir}/debug.log"
  statsfile="${bdir}/debug.ykstats"
  disfile="${bdir}/debug.dump"
  relstatsfile="${bdir}/release.ykstats"
  rellogfile="${bdir}/release.log"
  heatbc="${bdir}/heatmap.bytecodes.tsv"
  heatln="${bdir}/heatmap.lines.tsv"
  anchors="${bdir}/anchors.tsv"
  heatexec="${bdir}/heatmap.exec.tsv"
  rel_iters=$(( iters * REL_MULT ))
  printf "  %-12s %5d iters -> %s/\n" "${bench}" "${iters}" "${bdir}"

  # Bytecode disassembly (-d sends DUMP: lines to stderr). With the non-yk heatmap
  # binary, run the full iteration count first so the dump is annotated with
  # per-bytecode "[hits: N]" execution counts; then rank those into heatmap.exec.tsv.
  if [[ "${HEATMAP}" == 1 && -x "${HEATMAP_BINARY}" ]]; then
    hm_iters=$(( iters < HEATMAP_ITERS ? iters : HEATMAP_ITERS ))
    "${HEATMAP_BINARY}" -d -cp "${CP}" "${HARNESS}" "${bench}" 1 "${hm_iters}" \
      2>"${disfile}" >/dev/null || true
    awk '
      /^DUMP:.*>>.*= \(/ { m=$0; sub(/^DUMP:[ \t]*/,"",m); sub(/ =.*/,"",m); next }
      /\[hits:/ {
        h=$0;  sub(/.*\[hits: /,"",h);  sub(/\].*/,"",h)
        bc=$0; sub(/^DUMP:[ \t]*/,"",bc); sub(/ *\[hits:.*/,"",bc)
        if (h+0 > 0) printf "%d\t%s\t%s\n", h+0, m, bc
      }
    ' "${disfile}" 2>/dev/null | sort -rn >"${heatexec}" || true
  else
    "${BINARY}" -d -cp "${CP}" "${HARNESS}" "${bench}" 1 1 \
      2>"${disfile}" >/dev/null || true
  fi

  # hir,debugstrs: HIR instructions + source-location comments per trace.
  # Serialise compilation so all output is flushed before the next run.
  YKD_LOG_IR="${hirfile}:hir,debugstrs" \
    YKD_LOG="${logfile}:3" \
    YKD_LOG_STATS="${statsfile}" \
    YKD_SERIALISE_COMPILATION=1 \
    "${BINARY}" -cp "${CP}" "${HARNESS}" "${bench}" 1 "${iters}" \
    2>>"${logfile}" || echo "    [WARN] ${bench} exited non-zero (see ${logfile})"

  # debugstrs are comment lines (^; ) embedded in the hir file; extract separately.
  grep "^; " "${hirfile}" >"${dbgfile}" 2>/dev/null || true

  # --- Source-location heatmap from the trace debugstrs ------------------------
  # How many trace instructions map to each location. Drop "--- Begin ... ---"
  # section headers so trace anchors aren't double-counted into the body counts.
  grep -vE '^--- ' "${hirfile}" 2>/dev/null \
    | grep -hoE '[A-Za-z][A-Za-z0-9_]*\.som:[0-9]+:[A-Z_0-9]+' \
    | sort | uniq -c | sort -rn >"${heatbc}" || true
  grep -vE '^--- ' "${hirfile}" 2>/dev/null \
    | grep -hoE '[A-Za-z][A-Za-z0-9_]*\.som:[0-9]+' \
    | sort | uniq -c | sort -rn >"${heatln}" || true
  # Where root and side traces start: the hot anchors / guard-exit sites.
  grep -hoE '^--- Begin hir: [A-Za-z][A-Za-z0-9_]*\.som:[0-9]+:[A-Z_0-9]+' "${hirfile}" 2>/dev/null \
    | sed 's/^--- Begin hir: //' | sort | uniq -c | sort -rn >"${anchors}" || true

  # Copy each .som source that appears in the traces, plus a heat-annotated copy
  # (every source line prefixed with its trace hit count; "." = never traced).
  IFS=':' read -ra _cpdirs <<<"${CP}"
  while read -r sf; do
    [[ -z "${sf}" ]] && continue
    for d in "${_cpdirs[@]}"; do
      if [[ -f "${d}/${sf}" ]]; then
        cp -f "${d}/${sf}" "${bdir}/som/${sf}"
        awk -v base="${sf}" '
          FNR==NR { if (split($2,p,":")==2 && p[1]==base) hits[p[2]]=$1; next }
          { printf "%7s | %s\n", (hits[FNR] ? hits[FNR] : "."), $0 }
        ' "${heatln}" "${d}/${sf}" >"${bdir}/som/${sf}.heat" 2>/dev/null || true
        break
      fi
    done
  done < <(grep -hoE '[A-Za-z][A-Za-z0-9_]*\.som' "${hirfile}" 2>/dev/null | sort -u)

  if [[ -s "${hirfile}" ]]; then
    # release dumps "--- Begin hir ---"; debug (with debugstrs) dumps
    # "--- Begin hir: <loc> ---", so match the common prefix.
    traces=$(grep -c "^--- Begin hir" "${hirfile}" || true)
    printf "    %d trace(s) | hir %d lines | debugstrs %d lines | log %d lines | ykstats %d lines | dump %d lines\n" \
      "${traces}" "$(wc -l <"${hirfile}")" \
      "$(wc -l <"${dbgfile}")" \
      "$(wc -l <"${logfile}")" \
      "$(wc -l <"${statsfile}" 2>/dev/null || echo 0)" \
      "$(wc -l <"${disfile}" 2>/dev/null || echo 0)"
  else
    echo "    [WARN] no HIR output written"
  fi

  # Release pass: authoritative JIT coverage / abort counts (see header note).
  # YKD_LOG=:3 is REQUIRED — yk writes its log straight to that path, and with
  # YKD_LOG unset it defaults to Error-only verbosity, silently dropping the
  # Verbosity::Warning "Trace too long" abort lines (the log would be empty and
  # the abort grep below a false 0). SOM++'s own stdout/stderr is discarded.
  YKD_LOG="${rellogfile}:3" \
    YKD_LOG_STATS="${relstatsfile}" \
    "${REL_BINARY}" -cp "${CP}" "${HARNESS}" "${bench}" 1 "${rel_iters}" \
    >/dev/null 2>&1 \
    || echo "    [WARN] release ${bench} exited non-zero (see ${rellogfile})"

  rel_aborts="$(grep -c 'Trace too long' "${rellogfile}" 2>/dev/null || true)"
  printf "    release(%d iters): recorded_ok=%s recorded_err=%s compiled_ok=%s trace-too-long=%s\n" \
    "${rel_iters}" \
    "$(jstat "${relstatsfile}" traces_recorded_ok)" \
    "$(jstat "${relstatsfile}" traces_recorded_err)" \
    "$(jstat "${relstatsfile}" traces_compiled_ok)" \
    "${rel_aborts:-0}"
done

echo ""
echo "Done. Per-benchmark output under ${OUTDIR}/<bench>/"
echo "  Heatmaps:         heatmap.bytecodes.tsv, heatmap.lines.tsv, anchors.tsv"
echo "  Annotated source: som/<File>.som.heat"
echo "  Release ykstats:  release.ykstats (authoritative coverage)"
echo ""
echo "Top trace anchor per benchmark:"
for bench in "${BENCHES[@]}"; do
  printf "  %-12s %s\n" "${bench}" "$(head -1 "${OUTDIR}/${bench}/anchors.tsv" 2>/dev/null || echo 'n/a')"
done
