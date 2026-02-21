# Repository Instructions

This repository is part of a 3-repo signal pipeline. Keep code and docs aligned across all three:

1. AirspyHFDecimate
   - https://github.com/DonLakeFlyer/AirspyHFDecimate
2. airspyhf-zeromq (this repo)
   - https://github.com/DonLakeFlyer/airspyhf-zeromq
3. MavlinkTagController2
   - https://github.com/DonLakeFlyer/MavlinkTagController2

## System contract (cross-repo)

- `airspyhf-zeromq` publishes IQ over ZeroMQ PUB with the documented packet header and float32 IQ payload.
- `AirspyHFDecimate` subscribes to that ZeroMQ stream, validates header/sequence/rate, decimates, and emits UDP packets in the downstream expected format.
- `MavlinkTagController2` consumes downstream telemetry/detection products and relies on stream timing and packet continuity assumptions.

## Change policy

When changing protocol, timing, sample-rate assumptions, packet fields, or stream semantics in this repo:

- Treat it as a cross-repo contract change.
- Update README and CLI docs in this repo.
- Call out required companion changes in:
  - `AirspyHFDecimate` (subscriber/decimator side), and/or
  - `MavlinkTagController2` (consumer/controller side).
- Prefer backward-compatible transitions when practical (versioned headers/flags, feature flags, soft warnings before hard-fail).

## Integration checks to preserve

- Keep ZeroMQ header fields and endian/layout stable unless explicitly versioned.
- Keep sequence monotonicity and timestamp semantics stable.
- Preserve compatibility with decimator-side dropped/out-of-order/rate validation.

## Coding focus

- Keep edits minimal and protocol-safe.
- Do not introduce silent behavior changes in stream contracts.
- If ambiguity exists between repos, document assumptions in PR description and README notes.

## Versioning policy

- Any incompatible change to header fields, payload framing, timestamp semantics, or sequence behavior must be explicitly versioned.
- Prefer additive transitions: introduce new fields/flags in a compatible way before enforcing new behavior.
- Do not repurpose existing header fields without a version/compatibility gate.

## Wire-format and ABI contract

- Header field order, type widths, endianness, and payload layout are a strict contract with downstream repos.
- Avoid compiler-dependent binary layout assumptions as the sole transport definition.
- Keep serialization/parsing deterministic and documented.

## Rate and timing invariants

- Preserve sequence monotonicity (increment-by-one per packet under normal operation).
- Preserve timestamp semantics and monotonicity guarantees expected by decimator/consumer.
- Keep sample-rate field accurate and stable for downstream validation.

## Failure behavior matrix

- Define and document behavior for source-side anomalies and shutdown modes.
- Preserve compatibility with downstream anomaly detection for:
   - sequence gaps,
   - out-of-order/duplicate packets,
   - malformed header/payload,
   - rate mismatches.
- Any deliberate behavior change must include migration notes.

## Observability requirements

- Keep stable logging/metrics for sequence continuity, packet counters, and rate metadata.
- Ensure diagnostics are sufficient for downstream root-cause analysis.
- Use bounded-repeat logging patterns rather than unbounded spam.

## Cross-repo change checklist

When changing this repo, verify and document impact on:

- `AirspyHFDecimate`: parser assumptions, header version/size/field meanings, sequence/rate checks.
- `MavlinkTagController2`: downstream timing continuity and effects of altered publisher cadence.
- This repo README/CLI docs: option defaults, packet format details, and compatibility notes.

## Test requirements by change type

- Header/wire change: add/update format and compatibility tests.
- Timing/rate change: validate timestamp monotonicity and sample-rate reporting.
- Continuity/error change: verify gap/out-of-order behavior and diagnostic coverage.

## Performance and backpressure constraints

- Keep publisher behavior predictable under slow-consumer scenarios.
- Avoid hidden drops without explicit counters/logs.
- If batching/chunking changes, document downstream effects on latency and continuity.

## PR expectations

- Include a cross-repo impact section in PR descriptions.
- State backward-compatibility status and migration plan if needed.
- Include representative logs/metrics when changing stream or diagnostic behavior.
