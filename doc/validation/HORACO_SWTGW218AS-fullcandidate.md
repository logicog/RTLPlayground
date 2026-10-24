# HORACO SWTGW218AS ג€” Full Integration Candidate validation

Date: 22 October 2026

## Scope and identity

- Hardware: HORACO SWTGW218AS / RTL8373, eight copper ports and one SFP-labelled slot.
- Firmware reported by the operator: `RTLPlayground v0.1.0-fullcandidate`.
- SwitchLab: 0.7.1 with the corrected GETBULK dispatch/capability infrastructure.
- Tested firmware build artifact SHA-256: `235b78326a6a052eaf0c7f4ec0e8d7e2b3ba19d9236ee6d4a557dca642b2fe63` (build artifact identity; not a readback hash of the running Flash).
- This report separates measured hardware results from host/mocked validation and outstanding acceptance requirements.
- It does not certify earlier RTLPlayground builds or claim complete firmware acceptance.

## Ethernet: recorded laboratory results

The first 15 cases of the original acceptance run recorded 90 passing directional forwarding results: 3,127,832 frames sent and received, with zero reported capture-drop increase. Their overall child statuses were ERROR because a separate COM6 capture failed. Those historical statuses and raw evidence remain unchanged.

| Case | Frame bytes excluding FCS | Duration per direction | Target pps | Recorded sent / received | Ethernet sub-results |
|---|---:|---:|---:|---:|---|
| All ports: initial small frames | 60 | 10 s | 500 | 29,165 / 29,165 | 6/6 PASS |
| All ports: initial large frames | 1514 | 10 s | 500 | 29,649 / 29,649 | 6/6 PASS |
| Port sweep: small frames | 60 | 10 s | 500 | 28,797 / 28,797 | 6/6 PASS |
| Port sweep: large frames | 1514 | 10 s | 500 | 29,481 / 29,481 | 6/6 PASS |
| Port sweep: small frames | 60 | 10 s | 500 | 29,600 / 29,600 | 6/6 PASS |
| Port sweep: large frames | 1514 | 10 s | 500 | 29,724 / 29,724 | 6/6 PASS |
| Port sweep: small frames | 60 | 10 s | 500 | 29,741 / 29,741 | 6/6 PASS |
| Port sweep: large frames | 1514 | 10 s | 500 | 29,866 / 29,866 | 6/6 PASS |
| Baseline | 1514 | 60 s | 100 | 35,962 / 35,962 | 6/6 PASS |
| Frame size 60 | 60 | 60 s | 500 | 179,331 / 179,331 | 6/6 PASS |
| Frame size 128 | 128 | 60 s | 500 | 179,239 / 179,239 | 6/6 PASS |
| Frame size 512 | 512 | 60 s | 500 | 178,931 / 178,931 | 6/6 PASS |
| Frame size 1514 | 1514 | 60 s | 500 | 179,006 / 179,006 | 6/6 PASS |
| Generator qualification 1000 pps | 1514 | 60 s | 1000 | 348,656 / 348,656 | 6/6 PASS |
| Long-frame stability | 1514 | 600 s | 500 | 1,790,684 / 1,790,684 | 6/6 PASS |

## Focused rerun of interrupted small-frame stability case

The previous final case was interrupted by a host Npcap device-removal error. A new, independent run repeated all six directions using the original limits: 60-byte frames, 500 packets/s, 300 seconds per direction, zero allowed loss and 10% generator-rate tolerance. UART was included as receive-only capture; no SNMP/HTTP telemetry was added to the traffic test.

Focused Ethernet result: **PASS**. Native combined session status: `SKIP`; UART status: `SKIP`. These are separate measurements.

| Direction | Sent | Received | Lost | TX pps | Load qualification | Result |
|---|---:|---:|---:|---:|---|---|
| A->B | 147,231 | 147,231 | 0 | 490.77 | ACHIEVED | PASS |
| A->C | 146,974 | 146,974 | 0 | 489.91 | ACHIEVED | PASS |
| B->A | 149,841 | 149,841 | 0 | 499.47 | ACHIEVED | PASS |
| B->C | 149,573 | 149,573 | 0 | 498.58 | ACHIEVED | PASS |
| C->A | 147,046 | 147,046 | 0 | 490.15 | ACHIEVED | PASS |
| C->B | 146,130 | 146,130 | 0 | 487.10 | ACHIEVED | PASS |

Learned-unicast isolation checks: 6/6 PASS.
Final retest aggregate: **886,795 sent / 886,795 received**, zero loss, corruption or duplicates. All six source/destination rate qualifications passed. Npcap dropped/interface-dropped deltas were zero on A, B and C. The three saved PCAPs passed structural/truncation checks; each retains the first 10,000 frames, while the runner counts and validates all observed probes.

Evidence identifiers: original acceptance `6279d483-d854-41b0-ba7d-b675f032134e`; focused retest `102db519-c1e8-487d-81e6-8f4061b5748d`.

These are sequential directed tests through the declared anchor/paired-port topology, not every possible port pair, simultaneous full duplex, or wire-rate throughput.

## SNMP: recorded hardware tests for fullcandidate

| Test | Recorded result | Scope |
|---|---|---|
| Basic GET | PASS, 4/4 | Native HC octets, link state and speed, including link-down speed 0 |
| MultiGET | PASS | Four bindings in one matching response, correct order and types |
| Unknown OID | PASS | noSuchObject exception |
| Wrong community | PASS | No response |
| Malformed BER | PASS | No response |
| Oversized valid request | PASS | tooBig, error index 0, empty bindings, matching request ID |
| Wildcard UDP reply routing | PASS | 16 responses verified for two client source ports |
| 15-minute stability | PASS | 3,600/3,600 requests, no timeout-like errors or counter decreases |

Stress latency: mean 25.08 ms; P95 41.29 ms; maximum 119.56 ms. This was the earlier hardware acceptance run, not a new stress test during the focused Ethernet rerun. The legacy script prints an old firmware label; it is hardcoded metadata, not firmware discovery.

GETNEXT did not receive a response in its recorded probe and remains unresolved. The recorded walk failed during host socket Send before response decoding. Neither is counted as PASS. The offline request-builder check does not substitute for hardware evidence.

GETBULK is implemented by this candidate and is now an executable, version/model-gated SwitchLab test requiring all four scenarios, exit 0 and RESULT: PASS=4 FAIL=0. No GETBULK hardware PASS was produced by this focused Ethernet task.

## Host and build validation (not hardware measurements)

- Saved fullcandidate GCC 13.3 and Clang 18.1 ASAN/UBSAN suites: PASS with warnings treated as errors.
- Five saved Node GUI/privacy suites: PASS; they cover configuration, persistence and masking with fixtures.
- SDCC 4.5.0 strict builds: RTL8372 and RTL8373 SNMP on/off, plus HORACO SNMP on, all packaged successfully.
- Each packaged validation image is 524,288 bytes. No new firmware image was flashed during this test.
- Tightest saved candidate code bank: BANK3 48,561 / 49,152 bytes; 591 bytes free. Static stack capacity 134 bytes is not runtime high-water.

## UART and outstanding acceptance requirements

The focused run captured 0 UART bytes. No UART command, break or reset was sent. An opened but quiet console does not establish stack high-water or bank-switching safety.
- Operator explanation for the earlier COM6 failure: another application held the port. The historical generic error did not preserve that native cause.
- Required outstanding evidence includes a successful full walk/GETNEXT, actual GETBULK hardware results, denied-source ACL, SFR recovery, runtime stack high-water and firmware-associated bank tests.
- Browser geometry and full NMS interoperability remain outside the passing evidence reported here.
- UART failures must remain separate from valid Ethernet sub-results. Missing UART-dependent stack/bank evidence still blocks those required acceptance checks.

## Disposition

Full firmware acceptance remains incomplete. Passing Ethernet or basic SNMP tests do not waive any missing protocol, fault-recovery or runtime-memory validation.

Raw packet/UART artifacts and original acceptance records are retained locally. This public document contains sanitized aggregate results; no community value or raw packet payload is published.

## Related public work

- [SNMPv2c agent PR #615](https://github.com/logicog/RTLPlayground/pull/615)
- [Full-candidate SNMP follow-up proposal](https://github.com/avicarmeli/RTLPlayground/pull/1)

## 22 October publication preparation (historical)

The publication integration retains the original PR #615 history, merges upstream
`c9e8403c6e2845e5cf9b3bb2f96d1995dc7a6bf0`, and retains Andre Brait's four
original follow-up commits through `4803a8338b8206d5a8c2779333c3977d24370482`.
A separate rebuild of the original source snapshot reproduced the hardware-tested
SHA-256 above exactly, using SDCC 4.5.0 #15242, `GIT_VERSION=fullcandidate` and
`SOURCE_DATE_EPOCH=1792051732`. The original snapshot was not a Git commit.

The 22 October publication tree (`ad0e297`) is deliberately different: it omits temporary
`banktest` diagnostics and unrelated dashboard/translation edits, preserves
upstream NIC tests, and keeps Andre Brait's scoped System-card styling.
SNMP engine, hardware adapter and control source are unchanged apart from line
endings; the generic UDP difference is a comment. Port helpers retain the tested
BANK3 placement. New host/browser/build results apply to the publication tree;
the physical observations above apply only to the identified original image.
The proposed publication binary has not been flashed or hardware accepted.

The publication browser suite checks 24 enabled/disabled/unsupported layout cases
at 320, 360, 640, 760, 800, 1024, 1280 and 1600 pixels in headless Chrome. It checks
card overflow relative to the upstream page, not an absence of all pre-existing
page overflow. This does not resolve pending device interoperability, hardware
GETBULK/walk, recovery, banking or runtime stack requirements.

The 22 October publication preparation also reran GCC 13.3 / Clang 18.1 ASAN/UBSAN host tests
with `-Werror`, five Node GUI suites, and the same 24 browser cases against
HTML/JavaScript extracted from a packaged image: all passed. Eight SDCC 4.5.0
strict images passed: RTL8372 KP 6-port, RTL8373 KP V3.2 and exact HORACO with
SNMP off/on; CI's RTL8373 KP V3.1 with SNMP on; and RTL8372 KP 6-port with
HEALTH on and SNMP off. Each image is 512 KiB with valid CRC16.

For normal SNMP-on builds BANK1/BANK2/BANK3 use 41,013 / 46,856 / 48,561 bytes;
XDATA is 16,998 bytes and static stack capacity is 134 bytes. BANK2 is 89 bytes
smaller than the tested image; the other listed allocations are unchanged.
The full machine-definition check is **not passing**: its existing recipe
requires Bash, and `ONT_S207CW_62TS_SE` has an overlong model initializer.
The same SDCC error 147 was reproduced on untouched upstream main. A continued
per-profile compile survey passed 52 of 53 definitions; this is not 53 passing
firmware builds. No unrelated profile or CI expectation was changed.
