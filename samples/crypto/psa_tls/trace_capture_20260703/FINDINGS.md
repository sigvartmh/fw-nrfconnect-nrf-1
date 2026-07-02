# IronSide SE psa_call hang during TLS 1.3 handshake — investigation findings

Date: 2026-07-03. Target: nrf54h20dk/nrf54h20/cpuapp, psa_tls TLS 1.3 client,
`CONFIG_PSA_SSF_CRYPTO_CLIENT=y` (IronSide SE crypto service).
Companion artifacts in this directory; reproducer projects in
`samples/crypto/reproducer_sha_384` and `samples/crypto/reproducer_replay`.

## ROOT CAUSE (established 2026-07-03, after the elimination campaign below)

**A lost bellboard doorbell caused by a dequeue-after-notify race in the
IronSide SE call server** (`sdk-secdom/subsys/ironside_call/ironside_call_server.c`).

`process_queued_calls()` notifies the client (`mbox_send_dt`) *before*
removing the serviced slot from its request queue (`dequeue()`):

```c
        rsp->status = handle_call((nrf_owner_t)client->owner_id, &req, rsp);
        ...
        sys_cache_data_flush_range(buf, sizeof(*buf));
        mbox_send_dt(&client->mbox_tx, NULL);    /* client wakes here      */
        irq_lock_key = irq_lock();
        dequeue();                               /* slot dequeued later    */
        irq_unlock(irq_lock_key);
```

A fast client can receive the response, release the slot, allocate it again,
write the next request and ring the SE's bell all inside that window. The
SE's bellboard ISR (`ironside_call_recv`) then skips the slot because it is
*still queued from the previous request*:

```c
        if (is_queued(q_index)) {
                /* Must not cache-invalidate this buffer. */
                continue;                        /* new request dropped    */
        }
```

The new request is never enqueued, `ironside_call_sem` is never given, the
server thread dequeues the stale entry and sleeps. The request stays in
shared memory with `status=REQ` forever: no response, no fault report.

**Proof by experiment:** on a hung device, a single debug-probe write of `1`
to `cpusec_bellboard.TASKS_TRIGGER[12]` (address `0x5f099030`, the app-to-SE
IPC bell) immediately unwedged the SE: the frozen request was serviced, the
application resumed, and the previously-hung TLS handshake completed
end-to-end (client Finished observed at the openssl server).

This mechanism explains every observation recorded below: stochastic per-run
outcome (client wake-to-rering turnaround racing the SE's notify-to-dequeue
window), probabilistic suppression by anything that slows the client between
calls (console tracing most of all), insensitivity to memory layout, cache
configuration and alignment, and the freeze always landing on a fast
back-to-back same-slot call (the transcript-hash finish immediately follows
two quick IMPORT_KEYs).

**Suggested fix (SE side):** dequeue before notifying —
`flush → dequeue → mbox_send_dt`. The ISR skip-logic remains sound: in the
new ordering the slot holds a flushed response (`status < REQ`) during the
window, so a concurrent ISR scan neither skips a live request nor enqueues a
completed one. Alternative/belt-and-braces: after `dequeue()`, invalidate and
re-read the slot status and enqueue it again if it already holds a new `REQ`.

The client side (`zephyr/modules/hal_nordic/ironside/se/call.c` and the SSF
PSA client) was audited and is correct; no stack-lifetime or cache-coherency
defect exists on the application side.

---

The sections below document the elimination campaign that led to the root
cause, in chronological order. Two intermediate narratives (layout
sensitivity, SHA-384 specificity) are corrected by the final data.

## Executive summary

During the TLS 1.3 handshake, a PSA crypto request dispatched to IronSide SE
never receives a response: the request sits in IPC slot 0 (0x2f88fb80) with
status `IRONSIDE_SE_CALL_STATUS_REQ`, the SE never writes a response status or
rings the reply bellboard, and the SE event-report region (0x2f88fc00) stays
all-zero. The application core blocks forever in
`ironside_se_call_dispatch() -> k_event_wait()`.

The hang:

- always freezes at the **same call**: `psa_hash_finish` (TF-M function_id
  0x305) on a **clone** (op handle 3) of the handshake-transcript hash,
  issued from `mbedtls_ssl_get_handshake_transcript` while verifying the
  server Finished message — immediately after the ECDHE / HKDF / AEAD-decrypt
  / ECDSA-verify burst of the handshake;
- reproduces with both `TLS_AES_256_GCM_SHA384` and `TLS_AES_128_GCM_SHA256`
  (it is **not** SHA-384-specific; the 48-byte output in the frozen request is
  just mbedtls's `MBEDTLS_TLS1_3_MD_MAX_SIZE` buffer size);
- is **stochastic per run with a high hang probability** (~26 hangs vs 3
  passes across all configurations tested on one DK; passes appeared in three
  different configurations and did not repeat);
- is **unaffected by anything the application can change** (see elimination
  table) — including running with the app D-cache disabled and with a full
  D-cache clean+invalidate before every psa_call;
- happens while the **application-side request is provably valid**: the frozen
  request was captured three times (twice via post-hang probe reads, once via
  live IPC-slot sampling with an unmodified binary) with identical, correct
  iovec contents.

Conclusion by elimination: IronSide SE wedges internally while servicing this
request, dependent on SE-internal state produced by the preceding handshake
crypto traffic, and fails silently (no response, no fault report). App-side
factors — cache coherency, memory layout, alignment, call timing, call
stream — are exhaustively ruled out.

## The frozen request (identical in all captures)

```
slot0 @ 0x2f88fb80:  status=6 (REQ)  id=0 (PSA_V1)
args: handle=0x40000100 (TFM_CRYPTO)  type=0
      in_vec  @0x2f0138d0, in_len=1
      out_vec @0x2f013880, out_len=2
in_vec[0]:  base 0x2f0138e8 len 64   iov pack: op_handle=3, function_id=0x305
out_vec[0]: base 0x2f01394c len 4    (client-side operation handle writeback)
out_vec[1]: base 0x2f013978 len 48   (digest buffer, never written)
SE event report @0x2f88fc00: all zeros
rsp_evts.events=0x0e alloc_evts.events=0x0e (caller parked on slot-0 bit)
```

## Elimination table

Every row was tested with the deterministic harness (`trial.sh`: pristine
build -> flash -> fresh openssl s_server (TLS_AES_256_GCM_SHA384) -> eth-rtt
bridge -> reset -> classify PASS/HANG on serial output).

| Variant | Isolates | Runs | Result |
|---|---|---|---|
| unmodified | baseline | 5+ (incl. manual/user runs) | HANG |
| stack size +8/+16/+32/+64/+256/+4096 | all stack addresses / line offsets | 6 | HANG |
| passthrough `--wrap=psa_call` (tail call) | text insertion/shift | 1 | HANG |
| 32 KiB `.data` pad | data/bss displacement | 1 | HANG |
| busy-loop before each call | pure sub-us delay | 1 | HANG |
| 64 B fixed-buffer stores before each call | small cache dirtying | 1 | HANG |
| `k_cycle_get_32()` before each call | peripheral bus read | 1 | HANG |
| read request buffers before each call | request-line cache residency | 1 | HANG |
| rotating 32 KiB stores before each call | cache eviction pressure | 1 | HANG |
| `sys_cache_data_flush_and_invd_all()` before each call | full coherency each call | 1 | HANG |
| RAM ring tracer (`SAMPLE_PSA_CALL_TRACE_RAM`) | combination | 4 | 1 PASS / 3 HANG |
| UART tracer (`SAMPLE_PSA_CALL_TRACE`) | combination + ms delays | 1 | PASS (not repeated) |
| `CONFIG_DCACHE=n` | no app D-cache at all | 4 | 1 PASS / 3 HANG |

Reading: the scattered single passes (~10% of runs) are consistent with one
stochastic process across all configurations; no configuration reliably
suppresses or reliably differs from baseline. Two earlier bug-report
narratives are corrected by this data: the hang is neither
deterministic-per-binary/layout-sensitive nor SHA-384-specific, and
`CONFIG_DCACHE=n` does not fix it (single-run PASS was sampling noise).

## Additional negative results (synthetic reproducers)

- `samples/crypto/reproducer_sha_384`: replays the fatal call shape at
  escalating fidelity — same sizes, geometry (out_vec[0] at line offset 0x0c
  sharing a cache line with live words), dual persistent hash ops (clone gets
  handle 3), digest misalignment sweep 0-7 (exercising the bounce-buffer
  path), HKDF key schedule, ECDHE, AEAD record roundtrips, ECDSA
  sign/verify, RNG — and even the exact captured buffer addresses
  (0x2f01394c / 0x2f013978). **20k+ iterations per variant, zero hangs.**
- `samples/crypto/reproducer_replay`: replays the exact recorded psa_call
  stream (1126 calls, original addresses/lengths/content, generated by
  `gen_replay.py` from the captured trace) back-to-back through psa_call.
  **200 passes (~225k calls), zero hangs** (with expected status divergence on
  content-dependent calls).

Isolated PSA traffic does not reproduce the wedge; the failing state appears
to require the full TLS-handshake context on the SE side.

## Artifacts

- `trace_full.log` / `trace_parsed.txt` / `parse_trace.py` — complete
  psa_call trace of a passing handshake (UART tracer): 202 calls with all
  iovec addresses, 64-byte iov packs, statuses.
- `ring_dump_passing.txt` / `ring_parsed_passing.txt` / `parse_ring.py` —
  RAM-ring trace of a passing run with 1 MHz cycle timestamps (inter-call
  gaps 130-400 us, back-to-back).
- `ipc_watch_hanging_run.log` — live IPC-slot samples from a hanging run of
  an **unmodified** binary (captured via the eth-rtt-link-rs `ETH_RTT_IPC_WATCH`
  extension), ending at the frozen request.
- Instrumentation (all opt-in, in `samples/crypto/psa_tls`):
  `SAMPLE_PSA_CALL_TRACE` (UART tracer), `SAMPLE_PSA_CALL_TRACE_RAM`
  (RAM ring tracer, probe-dumpable), `SAMPLE_PSA_CALL_TRACE_NULL`,
  `SAMPLE_PSA_DATA_PAD`, `SAMPLE_PSA_CALL_PERTURB` (bisection aids).
- probe-rs chip description for non-invasive nRF54H20 cpuapp RAM reads while
  the core runs (no J-Link 54H20 support needed):
  `probe-rs read --chip nRF54H20-cpuapp-dbg --chip-description-path
  nrf54h20_cpuapp.yaml b32 0x2f88fb80 8`.

## Questions for the IronSide SE team

1. What SE-internal state can make servicing a `TFM_CRYPTO_HASH_FINISH` on a
   cloned multipart hash context (with two other live hash contexts and heavy
   preceding KD-op slot churn on the same op-slot index) block forever —
   without producing a fault report or response?
2. Is there a probabilistic element on the SE side (CRACEN context
   save/restore, DVFS transitions, KMU/TRNG interactions) consistent with a
   ~90% per-run wedge probability at this exact call?
3. Can the SE team reproduce with the psa_tls client
   (`overlays/client.conf;overlays/ecdsa_secp256r1.conf;overlays/tls_1_3_ecdsa.conf;overlays/cracen.conf`,
   nrf54h20dk/nrf54h20/cpuapp) against
   `openssl s_server -tls1_3 -ciphersuites TLS_AES_256_GCM_SHA384` with
   SE-side visibility? The wedge leaves an unlimited debug window (no
   timeout); the frozen request is always identical.
