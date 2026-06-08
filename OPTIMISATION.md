# Optimisations

## Yk locations for recursive calls (commit a8ccefe)

Previously, `InitYkLocs` only created `YkLocation`s for backward-jump targets
(loop headers). Recursive method calls — which are the SOM equivalent of a hot
loop for many algorithms — had no location attached, so the JIT never traced
them.

### What changed

1. **Recursion detection in `VMMethod`.** A new `bool called` flag on
   `VMMethod` tracks whether the method is currently on the stack:
   - `VMMethod::Invoke` / `Invoke1` set `called = true` on entry. If the
     method is re-entered while `called` is already `true`, that's a recursive
     call, and a fresh `YkLocation` is lazily allocated at `yklocs[0]` (the
     method entry).
   - `Interpreter::popFrame` clears `called = false` on return.
   - The flag is one-bit-conservative: `false` does not prove "not recursive",
     but `true` always indicates a recursive re-entry. That's enough — the
     first recursive call installs the location, and every subsequent
     recursive call hits it.

2. **Skip the SOM standard library in `InitYkLocs`.** Methods whose source
   path contains `Smalltalk/` no longer get any locations allocated. The stdlib
   is rarely the hot path under benchmarks and skipping it cuts location
   bookkeeping cost.
