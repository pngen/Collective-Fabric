# Contributing to Collective Fabric

Thank you for your interest in contributing to Collective Fabric. Collective
Fabric is a vendor-neutral C++20 runtime that governs distributed accelerator
collectives across identity, membership, topology, planning, execution,
measurement, overlap, health, persistence, and recovery.

## How to contribute

- Open an issue to describe a defect, a design question, or an enhancement.
- Open a pull request with a focused change and a clear, neutral description.
- Keep changes scoped. If a change spans several concerns, split it.
- Prefer small, reviewable commits with neutral, public-facing subjects.

## System boundary

Collective Fabric owns one systems boundary:

> How should distributed accelerator collectives be described, selected,
> executed, measured, overlapped, validated, and recovered so the system knows
> which collective algorithm/path is appropriate, which participants and
> generations are authoritative, what actually happened, and whether the
> result is safe to commit?

Collective Fabric is *not* a replacement for NCCL, RCCL, oneCCL, MPI, SHARP,
UCX, or future vendor collective engines. It governs and measures the layer
around such engines through clean backend contracts and provides a
deterministic reference backend to prove the runtime independently.

## Engineering expectations

- The library targets C++20 and must stay dependency-free where practical.
- CUDA is an optional backend/proof surface, not a requirement of the core.
- Use strong, non-interchangeable identity and generation types. Do not use
  raw integer aliases where accidental cross-assignment is possible.
- Prefer deterministic behavior. Do not introduce hidden nondeterminism.
- Do not fabricate topology, telemetry, measurements, or hardware evidence.
  Represent unmeasurable physical facts as UNKNOWN.
- Keep protocol and persistence parsing bounded before allocation.
- Guard lifecycle transitions explicitly.
- Do not swallow failures.

## Validation

- The library is expected to build and test warning-clean under
  `/W4 /WX` with MSVC on Windows, and under equivalent warning settings on
  other toolchains when available.
- Validate in both Release and Debug configurations.
- New code should add focused tests. Prefer many focused test executables over
  a single opaque monolithic test.
- Do not add test timeouts, watchdogs, or process-execution limits as a
  substitute for correctness.

## License

By contributing, you agree that your contributions are licensed under the
Apache License, Version 2.0. Copyright 2026 Summon Software Labs.
