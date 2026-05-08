# src/yk — Yk JIT integration

This directory contains all Yk-specific code for SOM++.

## Why a separate directory?

Keeping Yk code isolated here minimises the diff against upstream SOMpp. Upstream
files (`Interpreter.h`, `VMMethod.h`, `Universe.h`, …) include `YkSOMpp.h` behind
a single `#ifdef USE_YK` guard.
