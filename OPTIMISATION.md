# Method-entry Yk locations

## What changed

`VMMethod::InitYkLocs` (`src/yk/YkSOMpp.cpp:94`) now installs an extra
`yk_location_new()` at bytecode offset `0` of every user method, in addition to
the locations already placed at backward-jump targets. Methods loaded from the
SOM standard library (any source path containing `Smalltalk/`) are skipped.

## Why

Yk records traces between visits to the same control point. Previously the only
control points were loop headers — the targets of `BC_JUMP_BACKWARD` /
`BC_JUMP2_BACKWARD`. That works for methods whose hot path is a loop, but it
leaves a blind spot for **hot recursive methods that contain no backward
jumps**.

Adding a location at offset `0` turns every method entry into a candidate
control point. Once the entry counter trips, Yk records a trace covering the
method body (including the recursive call), and subsequent invocations execute
the compiled trace instead of the interpreter loop.

## Why skip the standard library

Locations are not free: each one carries a hotness counter and, once hot, a
compiled trace. Smalltalk-side library methods (`Integer>>+`, collection
helpers, etc.) are called from a huge variety of call sites with very different
argument shapes. Tracing them at entry would:

- waste compilation budget on traces that get invalidated by type mismatches,
- inflate the location table for methods that are better inlined into a
  caller's loop trace anyway.

Filtering on `Smalltalk/` in the source path keeps the optimisation focused on
user code, where recursion-driven hotness is the realistic case.
