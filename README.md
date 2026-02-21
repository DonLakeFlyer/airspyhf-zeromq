# airspyhf_zeromq_rx

`airspyhf_zeromq_rx` is a single-purpose receiver that streams Airspy HF+ IQ data over ZeroMQ PUB.

## Related repositories

- AirspyHFDecimate: [https://github.com/DonLakeFlyer/AirspyHFDecimate](https://github.com/DonLakeFlyer/AirspyHFDecimate)
- airspyhf-zeromq: [https://github.com/DonLakeFlyer/airspyhf-zeromq](https://github.com/DonLakeFlyer/airspyhf-zeromq)
- MavlinkTagController2: [https://github.com/DonLakeFlyer/MavlinkTagController2](https://github.com/DonLakeFlyer/MavlinkTagController2)

Protocol, packet-format, timing, and sample-rate assumption changes must be coordinated across all three repositories.

## What this repo builds

This project builds one executable:

- `airspyhf_zeromq_rx`

Default output is ZeroMQ on:

- `tcp://127.0.0.1:5555`

---

## Requirements

### Runtime/Build dependencies

- CMake (>= 3.12)
- C compiler
- `libzmq`
- `libairspyhf` (system-installed headers + library)
- `libusb-1.0`
- `pkg-config`

### Debian/Ubuntu example

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libusb-1.0-0-dev libzmq3-dev pkg-config libairspyhf-dev
```

### macOS example

```bash
brew install libusb zeromq pkg-config
```

You must also provide a system `libairspyhf` on macOS (package manager or local install).

---

## Build

### Recommended (Makefile wrapper)

```bash
git submodule update --init --recursive
make
```

This configures into `build/` and builds with CMake defaults (currently `RelWithDebInfo`).

Install:

```bash
sudo make install
```

### Raw CMake

```bash
git submodule update --init --recursive
cmake -S . -B build
cmake --build build --parallel
```

Install:

```bash
sudo cmake --install build
```

---

## Usage

### Minimal

```bash
airspyhf_zeromq_rx -f 146
```

### Explicit endpoint

```bash
airspyhf_zeromq_rx -f 146 -Z -I 127.0.0.1 -P 5555
```

### File output (disables ZeroMQ)

```bash
airspyhf_zeromq_rx -f 146 -r capture.iq
```

### Key options

- `-Z` enable ZeroMQ output (enabled by default)
- `-I <host>` bind host/IP (default `127.0.0.1`)
- `-P <port>` bind port (default `5555`)
- `-f <MHz>` tune frequency
- `-a <sample_rate>` sample rate
- `-g on|off` HF AGC
- `-t <0..8>` HF attenuator
- `-m on|off` HF LNA
- `-z` skip manual AGC/LNA commands
- `-r <file>` write raw IQ to file (switches from ZeroMQ)
- `-w` write WAV IQ file (switches from ZeroMQ)

---

## ZeroMQ packet format

Each PUB message is:

1. fixed header
2. IQ payload (`float32 I`, `float32 Q`, interleaved)

### Header fields (in order)

- `uint32 magic` (`0x5a514941`)
- `uint16 version` (`1`)
- `uint16 header_size`
- `uint64 sequence`
- `uint64 timestamp_us`
- `uint32 sample_rate`
- `uint32 sample_count`
- `uint32 payload_bytes`
- `uint32 flags`

Header size is currently 40 bytes.
The wire header is a packed, fixed-size layout with no compiler padding between fields.

### Semantics

- `sequence` increments by 1 per packet.
- `timestamp_us` is generated from a monotonic clock in microseconds.
- `flags & 0x1` indicates final chunk when finite transfer mode completes.

Use `sequence` gaps to detect packet loss.

---

## Testing

```bash
make test
```

Integration tests included:

- timestamp monotonicity + sequence continuity
- slow-consumer loss detection

Tests return skip (`77`) when no Airspy HF+ device is available.
