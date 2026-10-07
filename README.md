# thornk

[![Linux LTS (6.12) Compatibility](https://github.com/abit-foggy/thornk/actions/workflows/kbuild-lts.yml/badge.svg)](https://github.com/abit-foggy/thornk/actions/workflows/kbuild-lts.yml)
[![Linux Stable Compatibility](https://github.com/abit-foggy/thornk/actions/workflows/kbuild-stable.yml/badge.svg)](https://github.com/abit-foggy/thornk/actions/workflows/kbuild-stable.yml)

Linux Kbuild to Ninja / `thorn.build` converter powered by Pith and Thorn's embedded decompiler FFI.

`thornk` converts Linux kernel trees and Kbuild Makefiles into byte-deterministic Ninja build graphs (`build.ninja`) and `thorn.build` specifications. It evaluates `.config` options, resolves composite modules (`-objs`, `-y`, `-m`), handles multi-line continuations, recurses into subdirectories, recognizes native assembly (`.s` / `.S`), injects freestanding kernel flags, and filters unconfigured or test targets.

## Prerequisites

- [pith](file:///home/foggy/pith) (in `$PATH` or at `/home/foggy/pith/pith`)
- [thorn](file:///home/foggy/thorn) (at `/home/foggy/thorn`)
- [samu](file:///usr/bin/samu) or ninja

## Quick Start

### 1. Build thornk

```bash
make build
```

This compiles the standalone `bin/thornk` binary via Pith.

### 2. Convert a Linux Kernel Tree to Ninja

Convert a Linux kernel source tree into a Ninja build graph:

```bash
./thornk path/to/linux --config path/to/.config --arch x86 --out out/build.ninja
```

Options:
- `--config <path>`: Path to kernel `.config` (default: `<kernel_dir>/.config`)
- `--arch <arch>`: Target architecture (default: `x86`)
- `--out <file>`: Output Ninja or Thorn spec file (default: `build.ninja`)

### 3. Build with Samu / Ninja

```bash
samu -f out/build.ninja
```

### 4. Convert Single Kbuild / Makefile to `thorn.build`

```bash
./thornk path/to/Kbuild thorn.build
```

Override project name via environment:

```bash
PROJECT=my_driver ./thornk path/to/Kbuild thorn.build
```

## Running Tests

Run the acceptance and verification suites:

```bash
make test
```

Or run individual test scripts:

```bash
# Tree conversion and samu execution test
./test/test_thornk.sh

# Single file Kbuild conversion test
./tests/test_thornk.sh
```
