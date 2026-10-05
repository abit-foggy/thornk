# thornk

Kbuild to `thorn.build` converter powered by Pith JIT and Thorn's hookable decompiler engine.

`thornk` ingests Linux kernel `Kbuild` and `Makefile` trees, resolves multi-line continuations, composite modules (`-objs`, `-y`), and variable assignments, strips non-portable GCC compiler flags, injects kernel headers, and emits clean `thorn.build` specifications.

## Prerequisites

- [pith](file:///home/foggy/pith) (in `$PATH` or at `/home/foggy/pith/pith`)
- [thorn](file:///home/foggy/thorn) (at `/home/foggy/thorn`)

## Quick Start

Convert a Kbuild file to `thorn.build`:

```bash
./thornk path/to/Kbuild
```

Convert with custom output path:

```bash
./thornk path/to/Kbuild my_output.thorn
```

Run with Pith JIT directly:

```bash
pith run thornk.pi
```

Override project name via environment:

```bash
PROJECT=my_driver ./thornk path/to/Kbuild
```

## Generate Build Backends with Thorn

Once `thorn.build` is generated, compile it with Thorn to Ninja or POSIX Make:

```bash
# Generate build.ninja
thorn -f thorn.build --engine ninja

# Generate portable Makefile
thorn -f thorn.build --engine make

# Build with samu or ninja
samu -f build.ninja
```

## Running Tests

Run the test suite:

```bash
make test
# or
./tests/test_thornk.sh
```
