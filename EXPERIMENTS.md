# yksompp × Yk JIT — hypothesis & experiments

## Problem
Under the Yk meta-tracing JIT, yksompp regresses vs the SOM++ mark-sweep base on
send/alloc-heavy AWFY benches (from `bench.md`, June 30): Json +22%, Richards +16%,
Havlak +14%, CD +12% — while it *wins* on arithmetic-tight benches (List +20%,
Sieve +14%, Mandelbrot +12%).

## H0 — root-cause hypothesis
A Yk trace is a **linearized interpreter that still fetch-decode-dispatches every
bytecode**. The dominant, shared cost is the per-bytecode dispatch tax emitted by
`YK_DISPATCH_TRAMPOLINE`:
- per-bytecode `yk_promote` guards for `bc` (bytecodes ptr) and `big` (pc), each
  carrying a ~55-var `VMFrame` deopt snapshot,
- a `yk_mt_control_point` call,
- an un-foldable `load_bc` (`NOOPT_VAL`-barriered).

Evidence (debug HIR, June 30): `load_bc` is the #1 call in every trace (CD 47k,
Json 20k, Richards 13k); load+ptradd ≈ **42%** of trace instructions; guards ≈ **10%**,
each pinning ~55 vars (9–31M total var-spills). Benches with low arithmetic density
can't amortize the fixed per-bytecode cost, so they regress. Traced bytecode mix is
~20–26% sends + ~55–59% stack-shuffle + ~0% arithmetic.

## H1 — the lever under test (pc-threading)
The per-bytecode **`pc` promote-guard is a removable chunk of the tax.** `bc` is
already threaded (promoted only at frame changes/loop headers — removed ~half the
per-bytecode promote-guards). Do the same for `big` (pc):
- promote it to a trace constant only at **merge points** (loop headers, sends,
  returns, forward jumps, GC),
- advance it by a **compile-time constant** on straight-line code (`YK_ADVANCE_BIG`
  in `PROLOGUE`),
- **remove the per-bytecode `big = yk_promote(...)` from the trampoline.**

This is correctness-preserving because the control point keys off
`bytecodeIndexGlobal` (memory), not `big`; `big` is only the fetch index in
`load_bc(bc, big)`, and forward jumps re-sync it (`DISPATCH_JUMP`).

### Predictions
| signal | expected |
|--------|----------|
| guards per trace | ↓ |
| `total_deopt_var_spills` | ↓↓ (primary signal) |
| `load_bc` count | ≈ flat (barrier still blocks folding — **not** addressed here) |
| trace count / compile time | ≈ flat |
| runtime | small ↓, largest on dispatch/straight-line-heavy (Richards, CD); little on Havlak (alloc/GC) or Json (guard-churn) |
| correctness (AWFY verify) | unchanged |

## Experiments
- **E0 — baseline** → `traces-baseline/`: current working tree = committed code +
  `bc`-threading. (Send-fold `yk_promote(invokable)` and `yk_outline` were reverted,
  so they are **not** in E0.)
- **E1 — pc-threading** → `traces-pc-threaded/`: E0 + thread `big`
  (`YK_PROMOTE_BIG` / `YK_ADVANCE_BIG` / `DISPATCH_JUMP`; drop per-bytecode `big`
  promote from the trampoline). Rebuild + re-dump.

**Controls:** identical benches, iteration counts, build flags, and dump pipeline;
only the dispatch macros differ between E0 and E1. Compare per-bench:
`release.ykstats` (compile / jit-exec / outside-yk split, `traces_compiled_ok`,
trace-too-long aborts) and debug-HIR instruction mix (guards %, avg deopt-snapshot
vars/guard, total var-spills, `load_bc` count).

## Validity notes
- **Debug HIR overstates absolute counts** (more LLVM BBs/bytecode than release);
  use it for *ratios/mix*, use `release.ykstats` for timing and trace counts.
- **Havlak at 250 iters is not a verification checkpoint** ({1,15,150,1500,15000}),
  so the harness prints "No verification result for 250 found" and reports
  "incorrect result." This is a harness artifact — the **non-yk interpreter fails
  identically** — not a JIT miscompilation. For a real correctness check, run at
  1500 (expect `6102, 5213`) on both yk and non-yk.
- This lever alone will **not** close the 12–22% gap; it isolates one shared cost.
  Bigger remaining levers: operand-stack → SSA, frame/allocation elision, `load_bc`
  folding (relax `NOOPT_VAL`), Json single-char-string interning.

Reproduce: `OUTDIR=traces-baseline bash dump_traces.sh Richards CD Havlak Json`
(baseline) then apply H1 and dump to `OUTDIR=traces-pc-threaded`.

---

## E0 results (locked) + comparison vs June-30 (pre-`bc`-threading)

release.ykstats (authoritative) + debug-HIR mix. June-30 = old `traces/` dump.

| bench | metric | June-30 (pre-bc) | E0 (bc-threaded) | Δ |
|-------|--------|-----------------:|-----------------:|:---:|
| Richards | release traces | 256 | 254 | ~0 |
|          | compile s | 1.018 | 0.827 | **-19%** |
|          | jit-exec s | 9.178 | 9.440 | +3% |
|          | var-spills | 9.28M | 8.58M | -8% |
|          | load_bc | 12 725 | 12 781 | ~0 |
| CD       | release traces | 1007 | 706 | **-30%** |
|          | compile s | 3.980 | 2.337 | **-41%** |
|          | jit-exec s | 7.723 | 7.727 | ~0 |
|          | var-spills | 31.5M | 19.5M | **-38%** |
|          | load_bc | 47 276 | 31 554 | -33% |
| Havlak   | release traces | 162 | 147 | -9% |
|          | compile s | 0.623 | 0.488 | -22% |
|          | jit-exec s | 5.102 | 5.262 | +3% |
|          | var-spills | 4.18M | 3.47M | -17% |
| Json     | release traces | 459 | 199 | **-57%** |
|          | compile s | 1.812 | 0.911 | **-50%** |
|          | jit-exec s | 3.088 | 3.108 | ~0 |
|          | var-spills | 14.5M | 7.98M | **-45%** |
|          | load_bc | 19 838 | 11 102 | -44% |

### Key finding (updates H1's prediction)
`bc`-threading cut **compile time (-19…-50%), trace count (-30…-57%), and
var-spills (-8…-45%)** — but **`jit_executing` (steady-state runtime) stayed flat
(±3%) on all four.** So the per-bytecode **promote-guards are a compile-time /
trace-size cost, not a hot-loop runtime cost** (a non-failing guard is a cheap
compare; its deopt snapshot costs compile time + register pressure, not cycles).

Because AWFY wall-clock is steady-state-dominated (compile amortized over many
iterations), threading the guards does **not** move the `bench.md` gap. By the same
logic, **H1 (pc-threading) will further cut compile/guards/spills but is predicted
to leave runtime ~flat too** — it's a compile-health win, not a gap-closer.

The gap-closers are the **runtime** levers: operand-stack → SSA (the 42% memory
traffic), `load_bc` folding, and allocation elision.

---

## E2 — idempotent lookup folding (2026-07-02, IMPLEMENTED)

**Hypothesis.** perf on the E0+send-fold binary showed compiled traces still
*execute* `VMClass::LookupInvokable` + `Universe::GetGlobal` at runtime
(`yk_outline` keeps them opaque calls; only the *result* was promoted): Richards
10.1%+8.1% of CPU, CD 7.3%, Havlak 5.1%, Json 4.9%. yk's j2 compiler
(`ykrt/src/compile/j2/aot_to_hir.rs`, `is_idempotent()` path) replaces a call to
a `yk_idempotent` function whose args are all trace constants with the return
value recorded while tracing — the call vanishes from the compiled trace.

**Change.** `lookup_invokable_idem` / `get_global_idem` (`src/yk/YkSOMpp.cpp`):
`yk_idempotent` wrappers returning `uintptr_t` (ykllvm's recorder pass only
supports integer returns; it instruments *call sites*, so the recursive lookup
inside is fine). `doSend`/`doUnarySend` promote receiverClass + signature +
`Universe::invokablesEpoch`; `doPushGlobal` promotes globalName +
`Universe::globalsEpoch`. The fold emits **no guard of its own**, so soundness
comes from passing an *epoch* as an argument: `Universe::SetGlobal` (reached by
`system global:put:`) bumps `globalsEpoch`; `VMClass::SetInstanceInvokable(s)` +
the InstallPrimitive add-path bump `invokablesEpoch`; the epoch promote-guard
then deopts any trace holding a stale fold. `NOOPT_VAL(epoch)` in the wrappers
stops LTO dead-arg elimination from silently unsounding this. `doPushGlobal`
needs `yk_indirect_inline` — the inliner clones it into `Start()` and
OutlineUntraceable would otherwise mark the orphan `yk_outline`+promote (link
error).

**Trace-level result (Richards debug HIR, `traces-idem/`):** in-trace
`LookupInvokable` calls 3 447 → 27, `GetGlobal` 933 → 0, unfolded wrapper calls 8;
trace count (255) and compile time unchanged. perf after: both symbols gone from
the profile; [JIT] DSO share 40.6% → 55.7%.

**Wall-clock (median of 3, interleaved, `Harness <bench> 1 <iters>`):**

| bench | before | after | Δ |
|----------|-------:|-------:|------:|
| Richards | 8.65 s | 8.42 s | −2.7% |
| CD | 8.10 s | 8.15 s | +0.7% |
| Havlak | 7.15 s | 6.79 s | −5.1% |
| Json | 6.24 s | 6.00 s | −3.8% |

A variant that additionally re-promoted the folded result (belt-and-braces
devirtualization) was A/B'd and is equal-or-worse on Richards/CD/Havlak
(Richards −1.6%, CD +1.2%): with folds firing at >99% of send sites, the result
promote is a redundant guard plus one more recorder-shim call per interpreted
send. Dropped.

**Key finding.** Havlak/Json hit their perf-predicted ceiling; Richards did not:
its 18% of lookup samples were mostly *overlapped* latency (cheap cache-hit
lookups hidden under the trace's memory-bound body on an OoO core), so removing
the calls shifted samples into jitted code rather than freeing cycles. perf
sample share ≠ removable runtime. CD's small regression tracks its megamorphic
send churn (more promotes per send at interpreter/record time).

**Correctness.** TestSuite 221/221; Richards/CD/Json harness verification passes
(CD at <250 inner-iters fails identically on base + pure interpreter — harness
checkpoint artifact, not this change). Epoch soundness: `EpochTest.som`
(scratchpad) hot-compiles a `PUSH_GLOBAL` loop (2 traces), reassigns via
`system global:put:`, and observes the new value — stale-fold deopt works.

**Caveat.** Folds bake object pointers into trace code; sound for the non-moving
MARK_SWEEP heap (objects reachable via globals/class tables), revisit before any
moving GC.

**Full-suite rebench (haste diff 53 54, 99% confidence, full AWFY iters):**
vs sompp mark-and-sweep base, the June-30 regressions collapse — Json +22% →
+8.5%, Richards +16% → +8.9%, CD +12% → +9.2%, **Havlak +14% →
indistinguishable** — while the wins grow (List +31%, Towers +22%, Mandelbrot
+17%, Sieve +16%, Bounce +15%, NBody +10%). Long runs amortize compile time
better than the quick 3-rep A/Bs above. The residual is a uniform ~9% on
Richards/CD/Json: per-send frame allocation + GC + the linearized trace body.
Note the yk-side variance (Richards ±2120 ms vs base ±303) — JIT compile /
side-trace churn.

---

## E3 — trace into the frame machinery (2026-07-02, NEGATIVE — reverted)

**Hypothesis.** After E2, perf's top remaining bucket is the per-send frame
machinery (`NewFrame` + `AllocateObject` + memset + `Invoke*` +
`popFrameAndPushResult` ≈ 7–13%/bench). Annotation-only attack: `Invoke`/`Invoke1`
are only called *virtually*, and `OutlineUntraceable`'s call-graph walk ignores
indirect calls, so the whole chain (`Invoke*` → `PushNewFrame` → `SetFrame` →
`NewFrame` → `CopyArgumentsFrom`) is auto-outlined — every send in a trace ends
at an opaque indirect call. Marking the chain `yk_indirect_inline` (the pass's
escape hatch; each function individually — the walk doesn't treat annotated
functions as roots) lets traces specialize the frame set-up against the E2
constant callee. `popFrameAndPushResult` additionally needs `yk_unroll` (its
arg-pop loop blocks linearization; without it it stays an opaque call even
though it is directly reachable).

**Mechanically it worked:** release HIR (post-opt) showed `Invoke`, `Invoke1`,
`NewFrame`, `PushNewFrame`, `SetFrame`, `CopyArgumentsFrom`,
`popFrameAndPushResult` all gone from traces; only `AllocateObject` (the
allocation itself, deliberately opaque), 198 residual `load_bc` and 37 cold
`LookupInvokable` remained. Tests 221/221, 255 stable Richards traces, 0
record errors.

**But it is slower** (median of 3, vs the same pre-E2 baseline; E2 deltas for
comparison):

| bench | E3 Δ | E2 Δ |
|----------|------:|------:|
| Richards | +0.3% | −2.7% |
| CD | **+4.7%** | +0.7% |
| Havlak | −1.1% | −5.1% |
| Json | +1.2% | −3.8% |

**Why.** The opaque calls were cheap (predicted, hot in icache, one copy shared
by all sends); inlining their bodies into *every send site of every trace*
bloats traces (more instructions, more live state, worse icache) while the real
cost — `AllocateObject` + memset + the GC load from dead frames — survives
untouched. CD, with the most traces (706), pays the most. Same lesson as E2's
Richards shortfall: the frame *bookkeeping* was latency-overlapped; only the
allocation itself is additive cost, and no yk annotation can remove an
allocation (it is inherently non-idempotent; yk has no escape-analysis /
alloc-sinking hint). **Reverted; E2 is the shipped state.**

**Conclusion for lever #1:** within "strictly yk annotations" the frame bucket
is exhausted. Removing it requires either a VM-side LIFO frame pool or
allocation sinking implemented in yk's j2 optimizer (RPython-style virtuals) —
a yk-core feature, not an annotation.

---

## E4 — location flags & thresholds (2026-07-02, one keeper: YK_HOT_THRESHOLD=500)

Free-knob A/B on top of E2: build flags `YK_RECURSIVE_CALLS_LOC=false` (norec),
`YK_SKIP_SMALLTALK_STD=true` (nostd), both, plus env vars
`YK_SIDETRACE_THRESHOLD∈{20,50}` (default 5) and `YK_HOT_THRESHOLD=500`
(default 131) on the E2 binary. Median of 3, interleaved, vs E2:

| variant | Richards | CD | Havlak | Json |
|---------|---------:|---:|-------:|-----:|
| norec | −1.1% | **+91%** | +2.6% | +3.8% |
| nostd | flat | −0.6% | −0.9% | −0.5% |
| norec+nostd | flat | +88% | −0.3% | +0.7% |
| **hot500** | **−3.5%** | flat | −0.5% | −0.2% |
| st20 / st50 | −1.0% / +1.3% | flat | −0.4% | flat / +1.3% |

**Recursion anchors are load-bearing, not churn — hypothesis rejected.** CD
without them: `duration_outside_yk` 0.5 s → **10.9 s** (the recursive
tree-walks run interpreted; `recurse:` has no loops so it can never anchor a
root trace otherwise), 103 record errors, 265k trace entries/exits. Json (+3.8%,
recursive-descent parser) and Havlak (+2.6%, DFS) confirm. Keep
`YK_RECURSIVE_CALLS_LOC=true` forever; CD's 351 recurse-anchored traces are
coverage, not waste.

**`YK_HOT_THRESHOLD=500` is a keeper candidate:** Richards −3.5% with no
losses elsewhere. Mechanism (ykstats): 226 traces vs 255, compile 0.79 s vs
0.94 s, and — notably — `jit_executing` itself 8.06 s vs 8.42 s: marginal
locations never anchor, so hot loops stop fragmenting into competing traces.
Side-trace threshold (st20/st50) and nostd are a wash; leave defaults.

**hot500 shipping saga:** briefly baked into `YkUniverseInit`, then reverted
after a rebench datum showed `81996 / 83802 / 118582 ms` — an apparent +45%
tail. A follow-up quiet-box sweep (hot ∈ {131,250,500} × sidetrace ∈ {1,5,20},
8 reps each, fresh process per run) exonerated the threshold: hot500 is
**−4.5% mean with the tightest spread of the grid** (7.92–7.96 mean, max
8.05 s across 24 invocations; hot131 max 8.54) and sidetrace threshold is a
dead knob. The 118.6 s outlier coincided with directly-observed load 88–148 on
bencher16 (concurrent gcc + CSOM) — load contamination, not JIT
nondeterminism. Code keeps yk's default (131) for now; to settle it, rebench
on a quiet box with `YK_HOT_THRESHOLD=500` and ≥10 invocations, then bake 500
back into `YkUniverseInit` if the max stays boring.

---

## E5 — fold the lookup stragglers (2026-07-02, IMPLEMENTED)

E2's pattern extended to the three remaining lookup/dispatch sites:

1. **Super-sends** (`doSuperSend`): promote super + signature + invokablesEpoch,
   resolve via `lookup_invokable_idem`. Needs `yk_indirect_inline` alongside
   its `yk_unroll` (promotes + inliner-orphan risk, as with doPushGlobal).
2. **Block classes** (`Universe::NewBlock`): new `get_block_class_idem`
   wrapper; numArgs is a per-site constant. Lazy first-load goes through
   SetGlobal → bumps globalsEpoch, so the existing epoch guards it.
3. **Block dispatch** (`VMEvaluationPrimitive::Invoke/Invoke1`): these are only
   called virtually → auto-outlined; `yk_indirect_inline` makes them traceable
   and a promote of the block's method (per-site constant; the block *instance*
   varies but its method doesn't) devirtualizes the block body's Invoke inside
   traces. Unlike E3, the traced body here is ~10 instructions, not the frame
   machinery — and the A/B confirms it doesn't bloat.

**Richards release HIR:** residual `LookupInvokable` 37 → 0,
`GetBlockClassWithArgs` 25 → 0. Tests 221/221; EpochTest still passes.

**Wall-clock vs E2 (median of 3, interleaved):** CD −0.7%, Havlak −1.3%,
Json −1.5%, Richards +0.5% (noise) — small consistent wins exactly on the
block-heavy trio, as predicted. Kept in full.

---

## E6 — trivial-method trace-inlining (2026-07-02, KEPT for trivial methods; safe prims REVERTED)

Last family of opaque calls in traces: the tiny `Invoke*` bodies that are only
ever called virtually (auto-outlined). Two halves, A/B'd separately:

1. **Trivial methods** (`VMTrivialMethod.cpp`: `VMGetter`/`VMSetter`/
   `VMLiteralReturn`/`VMGlobalReturn`) — `yk_indirect_inline` (+ `yk_unroll`
   where the arg-pop loop needs linearizing; `numberOfArguments` is a
   per-invokable constant so unroll counts are stable). `VMGlobalReturn`
   additionally folds its `GetGlobal` via `get_global_idem`. **KEPT:**
   Richards −0.9% (tight: 8.29 s vs 8.37 s), CD/Havlak/Json flat. These bodies
   are pure loads/stores — tracing them in replaces an indirect call with
   straight-line field access.
2. **Safe primitives** (`VMSafePrimitive.cpp`: unary/binary/ternary) —
   same annotations + promote of the per-invokable `prim.pointer` to
   devirtualize the routine call. **REVERTED:** full package was CD +2.1%,
   Havlak +1.9%, Json +2.5% — the body still ends at an opaque call to the
   primitive routine, so tracing in added a promote-guard + trace bloat around
   an unavoidable call. (E3's lesson again, in miniature: inlining pays only
   when it *eliminates* the call, not when it decorates it.)

Tests 221/221 on both variants. Combined shipped state: E2 + E5 + E6(trivial).

---

## E7 — mutual-recursion anchors (2026-07-02, NEGATIVE — reverted)

**Hypothesis.** E4 showed recursion anchors are load-bearing coverage; the
anchor check in `VMMethod::Invoke*` only catches *direct* recursion. Extending
it to short-cycle mutual recursion (walk the top frames for `this`) should help
Json (recursive-descent parser, worst residual).

**Two variants tried:** (a) depth-4 frame walk per unanchored invoke; (b)
depth-2 walk gated by a 255-invocation probe budget per method
(`ykAnchorProbes`), so steady-state sends pay only the null-check.

**Result (vs E6b, median of 3):**

| variant | Richards | CD | Havlak | Json |
|---------|---------:|-----:|-------:|-----:|
| depth-4 | +1.4% | −0.9% | +0.7% | −0.4% |
| depth-2 + budget | +1.8% | −0.4% | −1.0% | flat |

**Why it fails.** The probe budget did NOT fix Richards, so the cost is not the
walk — it is the *anchors themselves*: marginally-mutually-recursive methods
(scheduler dispatch cycles) get entry anchors whose root traces fragment the
existing loop traces (the inverse of E4's hot500 effect). Json, the actual
target, is flat — its parser recursion is per-JSON-value, far colder than its
per-char loops, which are already anchored. Net negative; reverted.

**Rule refined:** anchors are coverage only where a cycle has *no other way* to
be compiled (direct recursion, loops). Adding anchors where coverage already
exists via an enclosing loop trace just fragments it.
