# yksompp

This is an experimental [yk-jit](https://github.com/ykjit/yk)-enabled [SOM++](https://github.com/SOM-st/SOMpp) interpreter.

## Building

First, initialise the core-lib submodule (SOM standard library):

```
$ git submodule update --init
```

Yk requires its own modified clang toolchain, supplied via `yk-config`. Update the `yk_config` path at the top of `justfile` to point to your installation.

To build with Yk support:

```shell
$ just build-yk        # debug build  → cmake-yk/SOM++
$ just build-yk-release  # release build → cmake-yk/SOM++
```

To build without Yk (plain SOM++):

```shell
$ just build          # release build → cmake-build/SOM++
$ just build-debug    # debug build   → cmake-debug/SOM++
```

## Running

```shell
$ just hello-yk       # run Examples/Hello.som under the JIT
$ just test-yk        # run the full SOM test suite under the JIT
```

To run an Are-We-Fast-Yet benchmark directly:

```shell
$ cmake-yk/SOM++ -cp Smalltalk:Examples/AreWeFastYet Examples/AreWeFastYet/Harness.som <Benchmark> <iterations> <inner-iterations>
```

To compare all AWFY benchmarks plain vs Yk side-by-side:

```shell
$ just awfy-compare
```

## yk-related tips

 - Lower the hot-loop threshold so tracing fires quickly:

   ```
   YK_HOT_THRESHOLD=5 cmake-yk/SOM++ -cp Smalltalk Examples/Hello.som
   ```

 - Write tracing statistics to a JSON file:

   ```
   YKD_LOG_STATS=ykstats.json cmake-yk/SOM++ -cp Smalltalk:Examples/AreWeFastYet Examples/AreWeFastYet/Harness.som Sieve 5 1
   cat ykstats.json
   ```

   `traces_compiled_ok > 0` and `trace_executions > 0` confirm the JIT is working.

 - See `doc/Yk/` for a full account of the integration.

## Upstream SOM++

This fork tracks upstream [SOM++](https://github.com/SOM-st/SOMpp); see the upstream README and the `doc/` folder for details on the base interpreter, SOM standard library, GC options (`-DUSE_TAGGING`, `-DCACHE_INTEGER`, `-DGC_TYPE`), and the test suite.
