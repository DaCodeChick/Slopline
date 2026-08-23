# Modernization Ledger

Tracking what was replaced, why, what provides it now, and how behavior was verified, phase by
phase. Charter: `AGENTS.md`. Archaeology: `HOTLINE_MODERNIZATION_REPORT.md` + `audit/`.

## Phase overview

| Phase | Name | Status |
|---|---|---|
| 1 | Build foundation + `hotline::protocol` wire codec | **Complete** |
| 1b | AppWarrior `testing` component (framework test facility) | **Complete** |
| 2a | AppWarrior Core: big-endian helpers (`appwarrior::endian`) | **Complete** |
| 2 | AppWarrior Core foundation (types, containers, event/application model seams) | Recommended next |
| 3 | Protocol payloads + legacy auth codecs (scramble, HMAC key schedule, HOPE) | Planned |
| 4+ | Net/transport, server core, client core, tracker, UI backends | Planned |

---

## Phase 1 — Build foundation + `hotline::protocol` wire codec

### What changed

New build foundation:

- `CMakeLists.txt` — C++23, extensions off, `HOTLINE_WARNINGS_AS_ERRORS` (ON),
  `HOTLINE_BUILD_TESTS`, `HOTLINE_SANITIZE`; targets are independently buildable.
- `cmake/CompilerWarnings.cmake` — `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
  -Wshadow -Wformat=2 -Wundef -Wnull-dereference -Wuninitialized`, warnings-as-errors by default.
  Conversion warnings are deliberate: protocol code lives on explicit integer widths.
- `cmake/Sanitizers.cmake`, `CMakePresets.json` — `gcc`, `clang`, `asan` (ASan+UBSan) presets.

New library `hotline::protocol` (`src/hotline/protocol/`), pure C++23, no I/O; depends only on
`appwarrior::core` (endian helpers — moved to the framework in Phase 2a):

| Modern | Replaces (legacy, unmodified) |
|---|---|
| `constants.h` — `TransactionType : u16`, `FieldId : u16`, `AccessPrivilege : u8`, `UserOption`, `FolderDownloadAction`, tags/sizes | the three anonymous enums in `Apps/Common Files/HotlineClientServerCommon.h` |
| `appwarrior::endian` — byte-shift big-endian `read/write_u16be/u32be`, `four_cc` (originally `hotline/protocol/endian.h`; promoted to AppWarrior Core in Phase 2a) | `TB()`/`FB()` byte swaps scattered through the tree |
| `transaction.h/.cpp` — `TransactionHeader` + `encode_header`/`decode_header`/`try_decode_header`/`encode_transaction` | `STranHdr` + `_TNSendTran` + header receive in `AppWarrior/Source/Hardware/UTransact.cpp:13-24,907-975,983-1047` |
| `field_list.h/.cpp` — `FieldList`/`Field` + encode/decode + integer/string field helpers | `UFieldData` buffer layout + `AddField`/`AddInteger`/`AddCString`/`AddPString`/`GetInteger`/`GetPString`/`GetCString` in `AppWarrior/Source/Data/UFieldData.cpp` |
| `handshake.h/.cpp` — `ClientHandshake`/`ServerHandshakeReply` + encode/decode + validation policy | `UTransact::GetConnectStatus` (client) and `ReceiveEstablish`/`AcceptEstablish`/`RejectEstablish` (server) |

Tests (`tests/protocol/`, 39 cases): golden byte vectors, round-trips, boundary values,
malformed/truncated/trailing input, historical validation policy, encode-side limit checks.
The test harness now lives in the framework itself — promoted to the `appwarrior::testing`
component in Phase 1b below (deliberately dependency-free: a zero-fetch build stays fully
offline/auditable, and it can be swapped for doctest without touching test bodies if that ever
pays off).

### Preserved semantics (byte-for-byte)

- 20-byte big-endian transaction header; field lists `u16 count + {u16 id, u16 size, data}`
  with **no padding** (historical `ALIGN_FIELDS == 0`).
- Integer fields: 2 bytes iff value fits unsigned 0..65535 (the historical `AddInteger` bit test:
  `value & 0xFFFF0000`), else 4 bytes; decode accepts 1/2/4-byte forms (1-byte raw, 2-byte
  zero-extended, 4-byte).
- Text fields are raw bytes, no terminator (both `AddCString` and `AddPString` emit bare
  characters; the Pascal length byte is caller-buffer-local, not wire format).
- Duplicate field IDs 112/112 (`UserFlags`/`Visible`) and 114/114 (`ChatId`/`Number`) preserved
  verbatim; lookup is first-match in wire order (successor to the legacy sorted lookup table).
- Handshake: `'TRTP' 'HOTL' v1 sub2` (client, 12 bytes; transfers use sub 3),
  `'TRTP' + u32 error` (server, 8 bytes); `'NICK'` accepted as `'TRTP'` alias on both sides;
  version != 1 → reject reason 1; reject reason 0 normalized to 1.
- Header `flag` byte carried as-is (0 normally; 1..32 key-permutation index under encryption —
  crypto lands in a later phase).

### Deliberate, documented divergences (hardening; no valid peer affected)

- Decode rejects trailing bytes, truncated headers/entries, and declared sizes that overrun the
  input (the legacy reader walked its offset table and ignored garbage).
- Integer fields of size 3/5+/… are an explicit `DecodeError::invalid_integer_field_size` instead
  of a silent 0 (`UFieldData::GetInteger`'s default case).
- Encode permits zero-size fields, which the legacy emitter never produced but legacy readers
  parse without trouble.
- API shape: encode = value-returning, throws `std::length_error` on unrepresentable input
  (>65535 fields / >65535-byte field — the historical `AddField` limits); decode =
  `std::expected<T, DecodeError>`, never throws, never reads out of bounds. Dynamic-span decoders
  are named `try_decode_*` to avoid fixed/dynamic span overload traps.
- The legacy *receive policy* that kills the connection when `total_size`/`data_size` is 0 or
  above the cap (2 MB framework / 512 KB server) is **not** in the codec — it belongs to the
  connection layer (Phase: networking) and is documented there for later.

### Audit corrections discovered in this phase

1. Field-ID collision list corrected: **no 117/117 field collision** — transaction 117
   (`NotifyChatChangeUser`) vs field 117 (`IconId`) are different namespaces. Real collisions:
   112 and 114 only. (`HOTLINE_MODERNIZATION_REPORT.md` §24.5 fixed.)
2. `S_CipherAlg`/`C_CipherAlg` are **3777/3778**, not 3771/3772: the historical comments are
   arithmetic typos (`0x0EC1 = 3777`, `0x0EC2 = 3778`); the compiled hex literals, used
   identically by the tree's client and server, are authoritative. (Appendix B and
   `audit/06-protocol.md` fixed.)

### Verification evidence

- `cmake --preset gcc && cmake --build --preset gcc && ctest --preset gcc` → 39/39 pass,
  warnings-as-errors clean (GCC 16.2).
- Same under Clang 22.1: 39/39 pass, clean.
- `cmake --preset asan && … && ctest --preset asan` → 39/39 pass under ASan+UBSan, zero findings.
- Golden vectors reproduce the historical wire bytes: TRTP establish
  `54 52 54 50 48 4F 54 4C 00 01 00 02`, accept `54 52 54 50 00 00 00 00`, empty
  KeepConnectionAlive header `00 00 01 F4 00 00 00 01 …`, single-field body
  `00 01 00 64 00 01 78`, `myField_Vers=197` → `00 C5`.

### Unresolved / deferred

- Crypto layer (encrypted-transaction flag/permutation mechanics, `HLCrypt` key schedule,
  Blowfish/HMAC-MD5/SHA-1) — later phase; `flag` byte is already carried faithfully.
- Live-interop capture against hxd-style peers remains the report's recommendation before
  freezing the more subtle codecs (tracker counts, timestamp semantics).
- Text encoding: wire text stays raw bytes; MacRoman→UTF-8 conversion is a UI-boundary decision
  (report §24.1).

---

## Phase 1b — AppWarrior `testing` component

Promoted the Phase 1 test harness from `tests/support/` into the framework as the **first modern
AppWarrior component**: `src/appwarrior/testing/include/appwarrior/testing.h`, INTERFACE target
`appwarrior::testing`, registered from `src/appwarrior/CMakeLists.txt`.

**Why it belongs in the framework** (per AGENTS.md): a unit-test facility is shared infrastructure
every AppWarrior-based application (Hotline client/server/tracker) and every AppWarrior module
needs. Shipping it in the framework keeps builds dependency-free, gives the whole project one test
vocabulary, and is deliberately replaceable — nothing outside it depends on its internals.

**API:** namespace `appwarrior::test` holds `Case`/`registry()`/`Registrar`, `CheckFailed`,
`check_failed()`, `fail()`, `to_hex()`, `bytes_from_hex()`, `require_bytes()` and `run_all_tests()`
(returns 0/1, the body of every test executable's `main()`). Only the registration macros are
`AW_`-prefixed — `AW_TEST_CASE`, `AW_CHECK`, `AW_FAIL`, `AW_REQUIRE_BYTES`,
`AW_REQUIRE_BYTES_MSG` — because macros cannot be namespaced (the one justified prefix per
AGENTS.md).

**Changes:** protocol tests migrated to the new API (`#include "appwarrior/testing.h"`,
`using namespace appwarrior::test`); `tests/support/` removed; `tests/test_main.cpp` is now a
two-line `main()` calling `run_all_tests()`. Verification: 39/39 pass on the gcc, clang and
ASan/UBSan presets.

---

## Phase 2a — AppWarrior Core: big-endian helpers (`appwarrior::endian`)

Promoted the general-purpose byte-order primitives out of `hotline/protocol/endian.h` into
AppWarrior Core: `src/appwarrior/core/include/appwarrior/core/endian.h`, namespace
`appwarrior::endian`, exposed by the new INTERFACE target `appwarrior::core` (the core module's
first content).

**Why it belongs in the framework** (per AGENTS.md): byte-order handling must be centralized and
is not Hotline-specific — every AppWarrior binary-format codec (Hotline wire protocol, news
database, user records, `'AWRZ'`/`'HLNZ'`/`'harc'` resources) needs big-endian reads/writes.
`four_cc` lives alongside them: it encodes a FourCC tag (`'TRTP'`, `'HOTL'`, `'HTXF'`, ...) as
the big-endian u32 it appears as on the wire/disk.

**Changes:** `hotline::protocol` now depends on `appwarrior::core` (public link) and calls
`appwarrior::endian::*` directly — no shims, no re-exports (AGENTS.md: no compatibility theater).
The old `hotline/protocol/endian.h` is deleted. The endian tests moved to
`tests/appwarrior/test_endian.cpp` (40th case added: `four_cc` goldens). Verification: 40/40 pass
on the gcc, clang and ASan/UBSan presets.

## Recommended next phase

**Phase 2 — AppWarrior Core foundation (continues).** Per AGENTS.md the framework is preserved and
modernized; with `appwarrior::core` (endian) and `appwarrior::testing` landed, the next increments
are the typed-integer/`typedefs.h` replacement and the container layer (`CPtrList`/`CLinkedList`/
`CBoolArray`/`UIDVarArray`/`CPtrTree` verdicts from `audit/01`) reimplemented or replaced per the
per-container analysis, each with behavioral tests — using the framework's own
`appwarrior::testing` component. That unblocks server/client modernization (which are AppWarrior
apps) without touching any platform UI backend. Phase 3 then returns to pure protocol:
payload structs (`SMyFileInfo`, `SMyUserInfo`, `SMyUserAccess`, dates, GUID) and the
legacy auth codecs with golden vectors.
