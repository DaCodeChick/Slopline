# Modernization Ledger

Tracking what was replaced, why, what provides it now, and how behavior was verified, phase by
phase. Charter: `AGENTS.md`. Archaeology: `HOTLINE_MODERNIZATION_REPORT.md` + `audit/`.

## Phase overview

| Phase | Name | Status |
|---|---|---|
| 1 | Build foundation + `hotline::protocol` wire codec | **Complete** |
| 1b | AppWarrior `testing` component (framework test facility) | **Complete** |
| 2a | AppWarrior Core: big-endian helpers (`appwarrior::endian`) | **Complete** |
| 2 | AppWarrior Core foundation (types, bits/align, IVA1 reader, container verdicts) | **Complete** |
| 3 | Protocol payloads + legacy auth (digests/HMAC, key schedule, scramble, payload codecs) | **Complete** |
| 3b | Crypto completion (Blowfish OFB-64, encrypted transactions, HOPE login flow) | Recommended next |
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

## Phase 2 — AppWarrior Core: types, bits/align, IVA1 reader, container verdicts

### Delivered code (all in `appwarrior::core`, all tested)

| Modern | Legacy | Notes |
|---|---|---|
| `bits.h` — `appwarrior::bits::get/set/clear/invert_bit` | `UMemory::GetBit/SetBit/ClearBit/InvertBit` (`UMemory.h:135-158`) | **MSB-first** order preserved exactly (bit i = byte i/8, bit 7-(i%8)) — `SMyUserAccess` and Phase 3 codecs depend on it. Trap documented: `UBitString` used the opposite LSB-first order. |
| `align.h` — `appwarrior::align::up/down` (power-of-two, unsigned-integral concept) | `RoundUp2..64` / `RoundDown2..64` macros (`typedefs.h:240-252`) | C++26 will add `std::align_up`; target is C++23. |
| `ivar_array.h/.cpp` — bounded `'IVA1'` decoder (`decode`/`find`/`item_data`) | `UIDVarArray::Unflatten` + static `GetItem` (`UIDVarArray.cpp:391-473,479-543`) | Read-only by design (assets must stay readable; new needs use `std::map<uint32_t, std::vector<std::byte>>`). Safety validation mirrors the legacy validator; ID ordering is deliberately lenient (see below). |

### typedefs.h replacement verdict (`AppWarrior/Headers/typedefs.h`, 286 LOC)

| Legacy | Replacement | Evidence |
|---|---|---|
| `Int8..Uint64`, `Char8/16`, `Float32`, `fast_float` | `<cstdint>` / `float` / `double` directly — no framework aliases (aliases that only obscure standard types are removed) | `Uint32` ≈6,272 matches, `Uint8` ≈3,677 — but every modern consumer names `<cstdint>` types directly |
| `nil`, `true/false` macros | `nullptr`, built-in keywords | |
| `min/max/swap/abs/clamp/diff` templates | `std::min/std::max/std::swap/std::abs/std::clamp`; `diff` → `std::abs(a - b)` per use site | |
| `RANGE(num,min,max)` | `std::in_range<T>(value)` | 1 file |
| `HiWord/LoWord` macros | explicit `>> 16` / `& 0xFFFF` | 3 / 2 files |
| `RoundUp/RoundDown2..64` | `appwarrior::align::up/down` | 20 files |
| `swap_int`, `TB/FB`, `CONVERT_INTS` | `std::byteswap`; `appwarrior::endian` (Phase 2a); byte-order centralization | |
| `FORCE_CAST`, `BPTR/CPTR/WPTR` | removed — no `reinterpret_cast` overlay (AGENTS.md) | |
| `operator_new_size_t` hack, `USE_PRE_INCREMENT`, `USES_FILE_EXTENSIONS`, `#undef` blocks | removed — platform configuration lives in the backend layer (later phase) | |
| `scopekiller`/`scopekill` (RAII delete) | local `std::unique_ptr`/scope guard at each use site; not a framework facility | 29 files |
| `StValueChanger` (save/restore a variable) | small local RAII type at the use site; not a framework facility | 1 file |

### Container verdicts (implementation = documented replacements, not ports)

Per AGENTS.md ("no blind substitution"; a wrapper that makes `std::vector` look like a 1998
pointer list is not a framework abstraction), none of these earn a modern AppWarrior container:

| Legacy | App usage | Replacement at future call sites |
|---|---|---|
| `CBoolArray` (bit-packed, THdl-backed, single-item Move/Swap only) | ~0 (client header decl only) | **remove-entirely**; `std::vector<bool>`/`std::bitset` if a need ever appears |
| `CLinkedList`/`CLink` (intrusive, O(n) tail ops) | 9 files (server connection lists, client queues) | `std::list`/`std::deque`, or `std::vector` + swap-remove, per access pattern |
| `CPtrList`/`CVoidPtrList` (growable pointer array, 1-based, heapsort + binary search, cursor iteration) | 55 files — the workhorse | `std::vector<T>` / `std::vector<std::unique_ptr<T>>`; `std::sort` + `std::lower_bound` for the sorted-search pattern; **drop the 1-based convention**; iteration = range-for (cursor replaced by iterators/indices) |
| `CPtrTree<T>` (flat level-encoded tree: `{u16 level, u32 childCount, void*}` runs; per-parent O(n²) bubble sort) | 5 files (news/tracker tree models) | explicit node trees (`std::vector<std::unique_ptr<Node>>` per domain). The flat level-encoded layout is **not persisted anywhere** — no format codec needed. Contract notes for the replacements: children follow parent contiguously; `RemoveItem(removeChildTree=false)` promotes the first child; `Sort` orders direct siblings of one parent; level ≤ 65535. |
| `UIDVarArray` (ID→blob associative array, `'IVA1'` flatten) | 0 direct apps; internal: error-catalog reader | the `'IVA1'` *format* survives as `appwarrior::ivar` (bounded reader); the *container* is replaced by `std::map<uint32_t, std::vector<std::byte>>` for new needs |
| `UBitString` (LSB-first bit ops) | 0 (only `CBoolArray`) | **remove-entirely**; LSB-first trap documented in `bits.h` |

### New archaeology finding (corrects audit/01)

The shipped catalog `legacy/AppWarrior/Error Msgs/UError(1).dat` **violates its own documented
sorted-ID invariant**: its offset table is `..., 9, 13, 11, 12, 13` (duplicate ID 13, out of
order). The legacy `UIDVarArray::Unflatten` would reject it as corrupt; it shipped broken because
the Windows loader used the non-validating static `UIDVarArray::GetItem` (whose binary search
then silently mis-looked-up some IDs). Verified byte-for-byte; all 8 shipped catalogs were
decoded with the modern reader (7 valid + this one anomalous). Consequently the modern decoder
enforces safety (tag, truncation, count guard, monotonic in-bounds offsets) but is deliberately
lenient about ID ordering, preserves table order, and `find`/`item_data` return the first match.

### Verification

- `appwarrior::core` is now a STATIC library (first compiled code: `ivar_array.cpp`).
- 20 new test cases (60 total): bit-order goldens incl. the `SMyUserAccess` privilege-indexing
  case, alignment goldens, hand-built + **two real shipped catalog goldens** (embedded
  `UMemory(3).dat` and the anomalous `UError(1).dat`), truncation-prefix sweep, malformed-input
  cases, leniency cases.
- Independent sweep: all 8 real `.dat` catalogs decode (see above).
- gcc, clang and ASan/UBSan presets: 60/60 pass, warnings-as-errors clean.

## Phase 3 — Protocol payloads + legacy auth

### Delivered

**`hotline::crypto`** (`src/hotline/crypto/`, zero dependencies):

| Modern | Replaces | Verification |
|---|---|---|
| `md5.h/.cpp` — `Md5::digest` (RFC 1321, written from the RFC) | `HLMD5` internals | RFC 1321 vectors + block-boundary lengths cross-checked vs python hashlib |
| `sha1.h/.cpp` — `Sha1::digest` (FIPS 180-1) | `HLSha1` internals | FIPS 180-1 vectors + boundary lengths |
| `hmac.h` — generic `hmac<Hash>()` (RFC 2104) over a `message_digest` concept | `HLMD5::HMAC_XXX` / `HLSha1::HMAC_XXX` | RFC 2202 vectors, long-key pre-hash path, `HMAC-MD5("","")` (audit/06 §11.1) |
| `key_schedule.h` — `derive_login_keys<Hash>()` / `permute_key<Hash>()` | `HLCrypt::Init` / `PermEncodeKey` / `PermDecodeKey` (HLCrypt.cpp:16-74) | golden vectors from an independent python implementation (t1, t2, perm3, both hashes; zero-round identity) |

The key schedule reproduces the historical roles exactly: `t1 = HMAC(pw, sk)` twice,
`t2 = HMAC(pw, t1)`; client enc/dec = t2/t1, server = t1/t2; permute = `key = HMAC(sk, key)`
× n, in place. Hash choice is compile-time (negotiated at login), hence templates — no
runtime type erasure.

**`hotline::protocol` payloads** (`payload.h/.cpp`) + **auth** (`auth.h`):

| Codec | Legacy source | Wire form |
|---|---|---|
| `FileInfo` | `SMyFileInfo`, written by `BuildFileList` (HotlineServ.cpp:2183-2220) | 20-byte header: type/creator **u32 LE** (legacy-Intel raw host copy — FourCCs appear byte-reversed from Windows peers; Mac peers sent BE), fileSize/nameScript/nameSize **BE**, then name bytes |
| `UserInfo` | `SMyUserInfo`, `ProcessTran_GetUserNameList` (HotlineServTrans.cpp:1475-1515) | id u16 BE, icon i16 BE, flags u16 BE, nameSize u16 BE, name bytes |
| `AccessMask` | `SMyUserAccess` raw 8-byte copy (HotlineServTrans.cpp:3262, HotlineTasks.cpp:4988/5119) | 8 bytes; privilege p = byte p/8 bit 7-(p%8) — modeled as a u64 read big-endian, privilege p = bit (63-p) |
| `DateTimeStamp` | `SDateTimeStamp::Flatten` (UDateTime(W).cpp:136-150) | year u16 BE, msecs u16 BE, seconds u32 BE — **local time, within-year**: seconds since Jan 1 of `year`, msecs within the second |
| `Guid` | `SGUID` + `UGUID::Flatten` (UGUID(W).cpp:63-75) | time_low BE, time_mid BE, time_hi BE, then 8 raw bytes — the Microsoft UUID network form |
| `auth::scramble` | login/password bitwise-NOT (HotlineTasks.cpp:1494-1503; HotlineServTrans.cpp:1620-1623) | self-inverse byte NOT; documented server-side lowercase/CR-replacement/password-stays-scrambled behaviors |

### Endianness finding (refines report §24.4)

The `SMyUserAccess` "endianness hazard" is narrower than first reported: the legacy
`SetBit`/`ClearBit` are byte-based, so **the wire bytes for a given privilege set are
host-independent** (both legacy Mac and Intel builds emitted identical bytes). The hazard only
affects code that interpreted the two u32 *values* numerically across hosts. The modern
`AccessMask` (u64, big-endian byte mapping) sidesteps it entirely. `appwarrior::endian` gained
`read/write_u32le` (for the verified legacy-Intel `FileInfo` quirk) and `read/write_u64be`
(for `AccessMask`).

### Verification

- 21 new test cases (81 total): digest RFC vectors, HMAC RFC/oracle vectors, key-schedule
  oracle goldens, all five payload goldens + truncation/trailing sweeps, access-mask
  byte-position goldens, scramble round-trips.
- gcc, clang, ASan/UBSan presets: 81/81 pass, warnings-as-errors clean.

### Deferred / unresolved

- Blowfish OFB-64 (zero IV) and the encrypted-transaction stream layer with the `flag`
  key-permutation mechanics (`_TNSendTran`, UTransact.cpp:907-975) — Phase 3b.
- The full HOPE login exchange (SessionKey/MacAlg/CipherAlg negotiation, `HMAC(login|password,
  sessionKey)` digests, 16/20-byte digest fields) — Phase 3b.
- Login lowercasing and `\r`→`-` replacement are text-encoding-aware (UText::MakeLowercase);
  they land with the text layer.

### Recommended next phase

**Phase 3b — Crypto completion.** Finish the protocol-crypto spine: Blowfish OFB-64 (zero IV,
Eric Young self-test vectors from the legacy `HLBlowfishData.h` tables), the encrypted-transaction
stream layer with the `flag` key-permutation mechanics (`_TNSendTran`, UTransact.cpp:907-975 —
0/2/7/13 re-roll, 2-byte old-key prefix), and the full HOPE login exchange codec
(SessionKey/MacAlg/CipherAlg negotiation, `HMAC(login|password, sessionKey)` digest fields).
Still pure logic — no networking, no platform backends.
