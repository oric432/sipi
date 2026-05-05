# SIPI — SIP + RTP Proof-of-Concept

A minimal SIP User Agent Server (UAS) and RTP receiver in C++26, using low-level PJSIP, Boost.SML for per-call state machines, and raw Boost.Asio UDP sockets for RTP. Receives calls from a SIP client (e.g., MicroSIP) on LAN, negotiates PCMA/G.711 A-law audio, and echoes received RTP packets back to the caller.

## Quick Start

### Prerequisites

- C++26 compiler (clang 17+)
- CMake 3.23+
- Linux (tested on Ubuntu 22.04)

### Build

```bash
cmake --preset debug          # configure (downloads deps, ~40s first run)
cmake --build build -j$(nproc)  # build (first run compiles PJSIP, ~3-5 min)
```

Available presets: `debug`, `release`, `relwithdebinfo`. All use Ninja + clang/clang++, output to `build/`.

PJSIP is built from source via CMake `ExternalProject` on first build; subsequent builds are incremental. Library files are cached in `build/_pjsip_install/lib/`, so cleaning only `build/CMakeFiles` won't re-trigger the full PJSIP rebuild.

### Run

```bash
cd build
./sipi
```

The binary expects `config.toml` in the current working directory.

### Configuration

Create `config.toml` at the project root:

```toml
[sip]
bind_address = "0.0.0.0"
bind_port = 5060
public_address = "192.168.1.50"   # used in SDP o= and c= lines

[rtp]
bind_address = "0.0.0.0"
port_min = 40000
port_max = 41000

[logging]
level = "debug"
pjsip_level = 1 # 0 - error, 1 - warning, 2 - info, 3 - debug, 4 - trace
```

- **sip.public_address**: Advertised in SDP as the remote endpoint for RTP. Must be reachable by the caller.
- **rtp.port_min/port_max**: RTP port allocation range. Allocator picks the first available port in this range per call.

## Architecture

### Component Overview

| Component | Responsibility |
|-----------|-----------------|
| **SipEndpoint** | PJSIP initialization, UDP transport, event loop, Asio thread spawn |
| **SipModule** | PJSIP module callbacks (`on_rx_request`, `on_inv_state_changed`), INVITE/BYE dispatch |
| **CallManager** | Session map (call-id → CallSession), find/create/remove, event dispatch |
| **CallSession** | Per-call state machine and context ownership, event routing |
| **CallContext** | Concrete ICallContext interface, SDP negotiation, RTP I/O, SIP response sending |
| **CallStateMachine** | Boost.SML transition table (templated on context), state and event definitions |
| **SipResponder** | SIP response sending (100 Trying, 180 Ringing, 200 OK, 488 Not Acceptable, 487 Request Terminated) |
| **SdpNegotiator** | Remote SDP parsing via PJMEDIA, PCMA validation, local SDP generation |
| **RtpSession** | UDP socket bind, RTP packet receive, echo transmission |

### Threading Model

**PJSIP thread** (main thread):
- Runs `pjsip_endpt_handle_events()` loop in `SipEndpoint::run()`
- Executes `on_rx_request` and `on_inv_state_changed` callbacks
- Dialog lock held during callbacks; safe for PJSIP calls
- **Must not block** — return immediately from callbacks

**Asio thread** (spawned in `SipEndpoint::run()`):
- Runs `io_context.run()` to handle async RTP socket operations
- Independent of PJSIP; cross-thread communication via `io_context.post()`
- RtpSession socket operations (bind, recv, send) run here

**Synchronization**:
- CallContext/CallSession/SM live on PJSIP thread (created in `on_rx_request`)
- RtpSession runs socket ops on Asio thread via posted handlers
- No shared mutable state between threads; all changes via message posting

### Call Flow

#### Happy Path (INVITE → ACK → RTP → BYE)

1. **Incoming INVITE**:
   - `SipModule::on_rx_request()` called on PJSIP thread
   - Create UAS dialog and inv session
   - Dispatch `InviteReceived` event to state machine

2. **Trying state** (parse SDP):
   - Send 100 Trying response
   - `SdpNegotiator::parse_remote()` checks for audio + PCMA
   - If valid SDP → dispatch `SdpParsed` event
   - If invalid SDP → dispatch `SdpRejected` event, send 488 Not Acceptable

3. **RTP allocation**:
   - Post async RTP socket bind to Asio thread
   - Allocate port from configured range
   - On success → dispatch `RtpReady` event

4. **Ringing**:
   - Send 180 Ringing + 200 OK (with local SDP)
   - Enter Answered state, wait for ACK

5. **ACK received**:
   - `on_inv_state_changed(PJSIP_INV_STATE_CONFIRMED)` on PJSIP thread
   - Dispatch `AckReceived` event → Confirmed state
   - RTP socket actively receives packets; echoes back to caller

6. **BYE received**:
   - `on_rx_request(BYE)` on PJSIP thread
   - Dispatch `ByeReceived` event → Terminating state
   - Close RTP socket, send 200 OK to BYE
   - Delete CallSession

#### Rejection (unsupported codec or missing audio)

1. `SdpRejected` event in IncomingInvite state
2. Send 488 Not Acceptable
3. Transition to Failed state
4. Session deleted on call termination

#### CANCEL flow

1. **Incoming CANCEL** (before ACK):
   - `SipModule::on_rx_request(CANCEL)` on PJSIP thread
   - PJSIP inv layer auto-responds 200 OK to CANCEL
   - Dispatch `CancelReceived` event to state machine

2. **SM action**:
   - `ctx.close_rtp()` — close UDP socket, stop receiving
   - Send 487 Request Terminated to original INVITE
   - Transition to Terminating state

### State Machine

**States**: `Idle → IncomingInvite → Trying → Answered → Confirmed → Terminating` (or `Failed` on rejection)

**Events**:
- `InviteReceived` — incoming INVITE with parsed call-id
- `SdpParsed` — remote SDP parsed, audio + PCMA confirmed
- `SdpRejected` — remote SDP missing audio or uses unsupported codec
- `RtpReady` — RTP socket allocated and bound
- `AckReceived` — ACK received, call confirmed (from `on_inv_state_changed`)
- `ByeReceived` — BYE received or call timed out (from `on_inv_state_changed`)
- `CancelReceived` — CANCEL received
- `TransportError` — RTP socket allocation failed

**Guards**:
- `is_valid_invite` — inv session pointer not null
- `valid_sdp` — has audio section and supports PCMA
- `invalid_sdp` — no audio or unsupported codec

**Actions**:
- `send_trying()` — 100 Trying
- `parse_sdp()` — validate remote SDP, generate local SDP
- `open_rtp()` — allocate and bind RTP socket
- `send_ringing()` — 180 Ringing
- `send_ok()` — 200 OK with local SDP
- `send_reject(code)` — 4xx rejection
- `close_rtp()` — close socket and stop receiving
- `send_bye_ok()` — 200 OK to BYE

### Key Design Rules

1. **No pjsua-lib** — only low-level PJSIP API
2. **SML manages logic only** — PJSIP calls happen in `SipResponder`, `SdpNegotiator`, invoked from SM actions
3. **One SM per call** — no global state machine
4. **RTP independent of PJMEDIA transport** — raw Boost.Asio UDP socket
5. **SDP parsing via PJMEDIA** — `pjmedia_sdp_*` functions, not manual string parsing

## Building and Testing

### Unit Tests

Tests use Catch2 and verify individual components in isolation:

```bash
cmake --build build -j$(nproc)
cd build && ./sipi_tests
```

Test files:
- `tests/test_sdp_negotiator.cpp` — SDP parsing and validation
- `tests/test_call_state_machine.cpp` — state transitions and actions
- `tests/test_rtp_session.cpp` — socket binding and packet I/O

Each test provides a mock `ICallContext` to isolate the state machine from SIP/RTP I/O.

### Manual Testing with Baresip

Use the provided test script to automate baresip testing:

```bash
# Single call test (5 seconds)
./scripts/test_baresip.sh single

# Multiple concurrent calls (3 simultaneous calls, default)
./scripts/test_baresip.sh multi

# Custom: 5 concurrent calls, 10 seconds each
./scripts/test_baresip.sh multi --calls 5 --duration 10
```

The script will:
1. Launch baresip instances on localhost (ports 5081, 5082, etc.)
2. Have each call the sipi endpoint automatically
3. Hold the call for the specified duration
4. Exit cleanly and show results

**Expected sipi logs**:
```
[sip] [192.168.1.x:abcd] INVITE received
[sip] [call-id] UAS dialog and inv session created
[sip] [call-id] INVITE confirmed (ACK received)
[rtp] [call-id] listening on 192.168.1.50:40000
[rtp] [call-id] received packet, seq=1, ts=160, payload_size=160, total_packets=42
```

For each concurrent call, you should see:
- Unique call-id in logs
- Different RTP ports allocated (40000, 40001, etc.)
- No crashes or memory errors
- Clean shutdown on BYE

**Manual testing without script**:
1. Start sipi: `cd build && ./sipi`
2. Start a SIP client (e.g., MicroSIP or baresip) on the same LAN
3. Call the sipi endpoint (e.g., `sip:user@192.168.1.50:5060`)
4. sipi accepts the call, negotiates PCMA, opens RTP socket, and echoes packets back
5. Verify audio quality and check console logs

## Code Organization

```
src/
├── main.cpp                    # entry point, config loading, endpoint startup
├── Settings.hpp/.cpp          # TOML config parsing
├── sip/
│   ├── SipEndpoint.hpp/.cpp    # PJSIP init, transport, event loop
│   ├── SipModule.hpp/.cpp      # PJSIP module, INVITE/BYE dispatch
│   ├── SipResponder.hpp/.cpp   # response sending
│   ├── SdpNegotiator.hpp/.cpp  # SDP parsing and generation
│   └── SipStatusCodes.hpp      # SIP response status codes
├── call/
│   ├── CallManager.hpp/.cpp    # session map, lifecycle
│   ├── CallSession.hpp/.cpp    # SM wrapper, event dispatch
│   ├── CallContext.hpp/.cpp    # concrete ICallContext interface
│   ├── ICallContext.hpp        # abstract interface for SM actions
│   ├── CallStateMachine.hpp    # SML transition table
│   └── Events.hpp              # event struct definitions
└── rtp/
    └── RtpSession.hpp/.cpp     # UDP socket, RTP I/O

tests/
├── test_sdp_negotiator.cpp
├── test_call_state_machine.cpp
└── test_rtp_session.cpp
```

## Dependencies

- **PJSIP** — low-level SIP stack (built from source via ExternalProject)
- **PJMEDIA** — media utilities for SDP parsing (built with PJSIP)
- **Boost.SML** — state machine library (header-only)
- **Boost.Asio** — async I/O (from Boost, configured for asio + system libraries)
- **spdlog** — structured logging (header-only)
- **toml++** — TOML config parsing (header-only)
- **Catch2** — testing framework

All dependencies are fetched and built by CMake; no pre-installed packages required.