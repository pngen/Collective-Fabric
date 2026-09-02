# Collective Fabric

Collective Fabric is a vendor-neutral C++20 runtime that governs distributed accelerator collectives. It does not replace NCCL, RCCL, oneCCL, MPI, SHARP, UCX, or a future vendor collective engine. It governs and measures the layer *around* such engines through clean backend contracts, and provides a deterministic reference backend to prove the runtime independently.

## The systems question

Collective Fabric owns one systems boundary:

> How should distributed accelerator collectives be described, selected, executed, measured, overlapped, validated, and recovered so the system knows which collective algorithm/path is appropriate, which participants and generations are authoritative, what actually happened, and whether the result is safe to commit?

The architectural thesis is that collective communication is not merely a backend API call. A collective is governed distributed execution with explicit membership, topology, capabilities, algorithm choice, transport path, buffer semantics, generation authority, health, progress, failure state, evidence, and completion authority.

## Why collectives are a runtime boundary

A collective is not an opaque call into a backend. The runtime can prove which participants formed the collective, which generation was authoritative, which topology and capabilities were known, which algorithm/path was chosen and why, what evidence describes its execution, what failed, what became stale, what may safely retry, and which result is allowed to become real.

## Non-goal

Collective Fabric does not implement an optimized vendor collective engine. It is not a drop-in replacement for NCCL, RCCL, oneCCL, MPI, SHARP, or UCX. Those engines can be wrapped behind the backend contract without changing core collective semantics.

## Collective kinds

Supported collective kinds: `BARRIER`, `BROADCAST`, `REDUCE`, `ALL_REDUCE`, `ALL_GATHER`, `REDUCE_SCATTER`. Reduction operations: `SUM`, `PRODUCT`, `MIN`, `MAX`. Datatypes: `INT32`, `UINT32`, `INT64`, `FLOAT32`, `FLOAT64`, `BYTE`. Invalid operation/datatype combinations are rejected explicitly. The reference engine uses a fixed, documented reduction order (a left fold in rank order). For floating-point types this is deterministic but is not claimed to be mathematically associative.

## Group and membership generations

A `CollectiveGroup` is immutable per `GroupGeneration`. Membership changes produce a new generation; historical membership is never mutated in place. Rank assignment is deterministic and denseranked. Duplicate participant ids, duplicate ranks, rank holes, conflicting boot identities, and malformed lifecycle transitions are rejected. Lifecycle: `CREATING`, `READY`, `DEGRADED`, `RECONFIGURING`, `FAILED`, `RETIRED`.

## Topology and algorithm planning

Collective Fabric consumes its own portable topology representation, independent of any external topology library. Links carry link classes (`INTRA_PROCESS`, `SHARED_MEMORY`, `HOST_MEMORY`, `NVLINK_CLASS`, `RDMA_CLASS`, `PCIE`, `NETWORK`, `UNKNOWN`) and provenance (`MEASURED`, `REPORTED`, `DERIVED`, `SYNTHETIC`, `UNKNOWN`). Synthetic topology never masquerades as measured physical topology; unknown facts remain `UNKNOWN`.

The `Planner` consumes the descriptor, group, topology, backend capabilities, and any available measurements. Hard constraints eliminate invalid candidates first; remaining candidates are ranked by named decision factors (rank count, payload size, expected steps, expected byte movement, measured bandwidth/latency, topology structure, backend capability, health). There is no single opaque score. Tie-breaking is deterministic. `BACKEND_DEFAULT` is a delegation fallback, chosen only when no real algorithm is feasible. Every selection exposes an `explain_plan` with named factors.

## Authority and fencing

Authority is incarnation-scoped. A higher numeric generation belonging to an old `WorkerBootId` never fences a fresh incarnation; the boot identity is checked first. The `AuthorityGroundTruth` (epoch, live boot, group/collective/attempt/dispatch generations) supports explicit evaluation of claims, rejecting stale epoch, stale boot, stale group generation, stale collective generation, stale attempt, stale dispatch, and duplicate completion.

## Failure and reconfiguration

Failure is distributed state. `CollectiveFailure` records the failure class, affected participant/rank, generations, whether side effects may have occurred, whether retry is permitted, whether reconfiguration is required, and whether prior results are invalid. Retry uses a new `AttemptId`/`AttemptGeneration`; membership reconfiguration uses a new `GroupGeneration`. Old completions never resurrect a collective after failure or reconfiguration.

## Measurement and provenance

Measurements distinguish `THEORETICAL`, `BACKEND_REPORTED`, `MEASURED`, `SYNTHETIC`, and `UNKNOWN`. Throughput metrics define their numerator and denominator explicitly (`payload_throughput_bytes_per_sec`, `logical_collective_bytes_per_sec`, `estimated_link_bytes_per_sec`). No unqualified `bandwidth` metric is published. Physical measurements age: freshness is `CURRENT`, `STALE`, `REVALIDATION_REQUIRED`, or `UNKNOWN`. Recovery never turns a recovered physical measurement back into `CURRENT`.

## Overlap semantics

Collective Fabric models whether an individual plan can overlap safely and describes the evidence for actual overlap. It does not perform global arbitration among multiple competing collectives (that belongs to a future Collective Scheduler). Overlap reasoning is deterministic and exposed in the plan explanation.

## Health semantics

Collective-path health is tracked separately from generic device health, with `HEALTHY`, `DEGRADED`, `UNHEALTHY`, `UNKNOWN`. Health changes carry a `HealthGeneration` and provenance; old reports cannot overwrite fresh state. A single collective failure does not imply permanent hardware failure.

## Persistence and recovery

Persistence is versioned binary with a magic, version, bounded counts, deterministic encoding, a CRC-32 checksum, and a SHA-256 semantic digest. Writes are atomic (`temp -> flush -> close -> rename`). Truncation, corruption, checksum mismatch, impossible counts, duplicate ids, invalid enums, and trailing garbage are all rejected. Recovered live authority is cleared and recovered physical observations become `REVALIDATION_REQUIRED`.

## Reference backend

A deterministic in-process reference backend serves as an oracle for all supported operations, with deterministic ordered rank handling. It detects mismatched element count, datatype, operation, missing rank, duplicate rank contribution, stale generation, conflicting root, malformed buffer size, incompatible in-place usage, duplicate completion, and late stale completion.

## Real multiprocess proof

A real coordinator and real worker OS processes communicate over framed TCP. The frame format has a magic, protocol version, message kind, bounded payload length, and a CRC-32 checksum. Bad magic, unsupported version, oversized payload, truncation, checksum mismatch, invalid enum, impossible generation, duplicate identity, and trailing garbage are rejected. The proof demonstrates a real barrier and all-reduce over the protocol, persists state, kills a worker as a real OS process, detects loss, advances the epoch/generation, restarts the worker with a fresh boot, replays stale authority classes and rejects each, re-forms the group generation, runs a fresh collective, and on a fresh coordinator process recovers logical state with recovered measurements forced to `REVALIDATION_REQUIRED`.

## Real RTX 5090 proof

The validation host has one NVIDIA GeForce RTX 5090 (compute capability 12.0 / `sm_120`). The CUDA proof discovers the device, confirms compute capability 12.0, allocates device buffers, initializes deterministic rank-specific input, performs host-to-device copies, launches real CUDA transform and verify kernels, synchronizes, performs device-to-host copies, runs the reference all-reduce over the host-staged path, and verifies exact CPU-reference parity with no out-of-bounds or mismatch. Device memory is freed and clean CUDA error state is verified. Invalid buffer sizes and unsupported datatype combinations are rejected before execution.

The transport used on the CUDA path is `HOST_STAGED_TCP`. It is explicitly *not* GPUDirect, RDMA, NCCL, NVLink, peer access, or PCIe saturation, and it is not presented as optimized multi-GPU collective performance. Device free-memory telemetry is reported honestly (normal driver/context variation) rather than inventing exact equality.

## Synthetic multi-GPU / multi-node proof

Deterministic synthetic scenarios run through the same production planner and are explicitly labelled `SYNTHETIC`. They include two GPUs with a direct high-bandwidth peer-class link, four GPUs in a ring, four GPUs behind a switch-like shared domain, a two-node two-GPU-per-node hierarchy, asymmetric link bandwidth, a degraded link, shared-link contention, unknown link capability, an unavailable required backend, small-message latency-favoring and large-message bandwidth-favoring cases, topology generation rollover, stale health generation, deterministic identical-state ranking, and participant failure requiring reconfiguration. No synthetic scenario is presented as physical multi-GPU validation.

## Build and install

Requirements: a C++20 compiler (MSVC 19.44 or later on Windows, or GCC/Clang on other platforms), CMake 3.20+, and Ninja. CUDA is optional.

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ctest --test-dir build --output-on-failure

CUDA proof (optional):

    cmake -S . -B build-cuda -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCOLLECTIVEFABRIC_ENABLE_CUDA=ON \
      -DCMAKE_CUDA_COMPILER=<path-to-nvcc> \
      -DCOLLECTIVEFABRIC_CUDA_ARCHITECTURES=120   # sm_120 on the validation host
    cmake --build build-cuda
    ctest --test-dir build-cuda -R cuda --output-on-failure

CMake options: `COLLECTIVEFABRIC_ENABLE_CUDA`, `COLLECTIVEFABRIC_BUILD_TESTS`, `COLLECTIVEFABRIC_BUILD_EXAMPLES`, `COLLECTIVEFABRIC_BUILD_BENCHMARKS`, `COLLECTIVEFABRIC_BUILD_TOOLS`.

The core library is dependency-free C++20. CUDA is an optional proof surface, not a dependency of consumers of the core library. On the validation host, project-controlled CUDA code targets `sm_120`; compiler-language flags are kept scoped so C++ warning flags are not leaked into `nvcc`. Narrowly scoped suppression is used only for unavoidable vendor-generated CUDA host-stub warnings.

To install to a prefix:

    cmake --install build --prefix <prefix>

## Package consumption

Installed package (install to a clean prefix, then configure an independent consumer):

    find_package(CollectiveFabric CONFIG REQUIRED)
    target_link_libraries(my_tool PRIVATE CollectiveFabric::collectivefabric)

Import target: `CollectiveFabric::collectivefabric`. Headers live under `include/collectivefabric/`. Downstream consumers must use the installed package outside the source/build tree.

## CLI

    collective-fabric-cli discover
    collective-fabric-cli group-create 4
    collective-fabric-cli plan ALL_REDUCE FLOAT32 4
    collective-fabric-cli explain-plan ALL_REDUCE FLOAT32 4
    collective-fabric-cli execute-reference ALL_REDUCE FLOAT32 4
    collective-fabric-cli measure ALL_REDUCE FLOAT32 4
    collective-fabric-cli simulate
    collective-fabric-cli health
    collective-fabric-cli save state.bin
    collective-fabric-cli recover state.bin
    collective-fabric-cli benchmark 1000

CLI output makes provenance explicit, e.g. `provenance=MEASURED`, `provenance=DERIVED`, `provenance=SYNTHETIC`, `freshness=REVALIDATION_REQUIRED`.

## Examples

Runnable examples under `examples/` demonstrate real APIs (not CLI shell-outs). There are 14 examples: group identity, broadcast/reduce/all-reduce/all-gather/reduce-scatter reference, topology planning, algorithm selection, overlap reasoning, health and degradation, persistence and recovery, stale authority, multiprocess collective, and explain/replay. The CUDA host-staged collective proof is exercised by the `test_cuda` executable in the CUDA build.

## Benchmarks

Completed-work benchmarks under `benchmarks/` report explicit units (ops/s, µs/op, bytes/s, GiB/s) for group construction, descriptor canonicalization, plan selection, all-reduce and all-gather reference execution, explanation generation, persistence serialization and recovery, protocol encode/decode, concurrent planning, and measurement ingestion. Loop iterations are never reported as throughput unless each iteration performs a complete semantic operation. The host-staged CUDA/TCP collective is reported separately in the CUDA proof.

## Limitations

- The physical validation host has one NVIDIA GeForce RTX 5090; no multi-GPU CUDA collective was exercised.
- Optimized multi-GPU NCCL/RCCL execution was not physically exercised; no NCCL/RDMA/GPUDirect claim is made unless actually compiled and exercised.
- Multi-GPU and multi-node topologies are signalized as `SYNTHETIC` where not measured.
- `HOST_STAGED_TCP` is a host-staged path, not GPUDirect, RDMA, NCCL, or NVLink.
- Theoretical/derived estimates are not measured bandwidth or latency.
- Unknown physical capabilities remain `UNKNOWN`; they are never filled from datasheets or marketing specifications.

## Telemetry

Collective Fabric transmits no telemetry. All observations, logs, benchmarks, and persistence artifacts are stored locally on the host that produced them.

## License
Apache License 2.0. Copyright 2026 Summon Software Labs. No telemetry transmission.