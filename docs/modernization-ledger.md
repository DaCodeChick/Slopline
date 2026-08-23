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
| 3b | Crypto completion (Blowfish OFB-64, encrypted transactions, HOPE login) | **Complete** |
| 3c | `aw` namespace + framework/general-purpose promotions | **Complete** |
| 3d | AppWarrior consolidated into a single monolithic library | **Complete** |
| 4 | Transfer/archive/tracker codecs (FILP/RFLT/folder items/harc, tracker messages) | **Complete** |
| 4b | Shared-library AppWarrior + exception-free expected-based error handling | **Complete** |
| 4c | Configurable library shape (BUILD_SHARED_LIBS / BUILD_MONOLITHIC) + AW_API exports | **Complete** |
| 5 | Networking (aw::net transport, hotline::net connection + login session) | **Complete** |
| 5a | Networking follow-ups (WinSock, IPv6, AW_API, role-split connections) | **Complete** |
| 5b | Component gating (BUILD_CLIENT/BUILD_SERVER/BUILD_TRACKER) + UDP transport | **Complete** |
| 6 | Server core (listeners, dispatch, user/news DBs, agreement/banner) | Recommended next |


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

---

## Phase 3c — `aw` namespace + framework/general-purpose promotions

**Naming (C++ only):** the framework namespace is now the short prefix **`aw`** — `aw::endian`,
`aw::bits`, `aw::align`, `aw::ivar`, `aw::test`, `aw::crypto`, `aw::guid`, plus `aw::DecodeError`
at the root. The framework *name* remains **AppWarrior** (AGENTS.md charter); the CMake target is
`appwarrior` / `appwarrior::appwarrior` (single monolithic library since Phase 3d); the test
macros stay `AW_`-prefixed (macros cannot be namespaced). Rationale: `aw` is the code-level
brevity the same way the historical code leaned on `U`/`C` prefixes — scoping is handled by the
namespace, so the prefix stays short.

**Promotions into AppWarrior (general-purpose, not Hotline-specific):**

| Facility | New home | Hotline side |
|---|---|---|
| MD5, SHA-1, generic RFC 2104 HMAC + `message_digest` concept | `appwarrior/crypto/` — `aw::crypto` (new `appwarrior::crypto` library; the old `hotline::crypto` library is **deleted**) | — |
| `Guid` (Microsoft UUID network form codec) | `appwarrior/core/guid.h` — `aw::guid::Guid` | news GUID field (319) consumers use the framework type directly |
| `DecodeError` (truncated / trailing_bytes / invalid_integer_field_size) | `appwarrior/core/decode_error.h` — `aw::DecodeError` | `hotline/protocol/decode_error.h` re-exports it as `hotline::protocol::DecodeError` (one shared decode-error vocabulary) |
| Login key schedule (`HLCrypt::Init` t1/t2, `Perm*Key`) | — (it IS Hotline-specific) | `hotline/protocol/key_schedule.h` — `hotline::protocol::auth::derive_login_keys/permute_key`, templates over `aw::crypto::message_digest` |

`hotline::protocol` now links `appwarrior::core` + `appwarrior::crypto` (public). Blowfish will
land in `aw::crypto` in Phase 3b; the encrypted-transaction stream stays Hotline.

**Verification:** unchanged behavior — all 81 cases pass on gcc, clang and ASan/UBSan presets
after the restructure (pure rename/promotion; no semantic edits).

---

## Phase 3d — AppWarrior consolidated into a single monolithic library

Per project decision (AGENTS.md "AppWarrior Modularity", amended): the three component targets
(`appwarrior_core`, `appwarrior_crypto`, `appwarrior_testing`) are merged into **one STATIC
library** — CMake target `appwarrior`, alias `appwarrior::appwarrior` — with a single
`src/appwarrior/CMakeLists.txt`; the per-component CMakeLists are deleted. Consumers
(`hotline::protocol`, tests) link the one target.

The *component organization* survives at the source level (directories + `aw::` sub-namespaces:
`core/`, `crypto/`, `testing/`; future `ui/`, `platform/`), so the conceptual decomposition and
the platform-backend boundaries are unaffected. Because the library is static, the linker pulls
in only referenced objects — a headless Hotline server never links unused code, and future
GUI/backend objects cost nothing for consumers that don't use them. Do not reintroduce
per-component library targets.

**Verification:** unchanged behavior — 98/98 tests pass on gcc, clang and ASan/UBSan presets
with the single library.

---

## Phase 4b — Shared-library AppWarrior + exception-free expected-based errors

Two project decisions (AGENTS.md amended):

**1. AppWarrior ships as one SHARED library.** The `appwarrior` target is now `SHARED`
(`libappwarrior.so.0.1.0` with soname, `WINDOWS_EXPORT_ALL_SYMBOLS` for future Windows
builds). All Hotline applications link the one dynamic library. The component organization
(directories + `aw::` namespaces) is unchanged; note the shared library links all of its
objects, so the earlier static-linker argument in the Phase 3d note no longer applies — the
charter's Modularity section now states the shared-library decision explicitly.

**2. Production error handling is exception-free.** All 14 remaining `std::length_error`
throw sites (the encode paths in `field_list`, `payload`, `transfer`, `tracker`) were
converted to `std::expected<T, EncodeError>` with a new shared `aw::EncodeError`
(`element_too_large`, `count_too_large`, `string_too_long`; re-exported as
`hotline::protocol::EncodeError`). Decode already returned `std::expected`. Production
libraries now never throw for recoverable errors; the only remaining exceptions are the test
harness's assertion control flow (`aw::test::CheckFailed`, plus `bytes_from_hex` parse
failures) and standard-library facilities. Tests use a new `aw::test::unwrap()` helper so
expected-based call sites stay readable; the two `length_error` catch-tests now assert the
specific `EncodeError` values instead.

**Verification:** unchanged behavior — 122/122 tests pass on gcc, clang and ASan/UBSan
presets, now linking against `libappwarrior.so`.


---

## Phase 4 — Transfer/archive/tracker codecs

### Delivered (`hotline::protocol`, all byte-verified against legacy writers/readers)

| Codec | Legacy source | Wire form |
|---|---|---|
| `transfer.h/.cpp` — FILP flat-file package | `UFileSys(W).cpp:2180-2440` | 24-byte header ('FILP', version, 16 reserved, forkCount) + forkCount × {16-byte fork header + data}; INFO-then-DATA stream order; INFO fork = 72 fixed bytes + name + u16 commentSize + comment |
| `transfer.h/.cpp` — RFLT resume record | `UFileSys(W).cpp:2216-2226, 2310-2332` | 'RFLT', version 1, 34 reserved, u16 count, count × 16-byte entries; `data_resume_size()` mirrors ResumeFlatten (last DATA entry wins) |
| `transfer.h/.cpp` — folder-download items + commands | `HotlineServ.cpp:5500-5570`, `HotlineTasks.cpp:3480-3610` | item = u16 size + u16 type (bit0 folder) + u16 pathCount + {u16 script, u8 namelen, name} components (root excluded); ResumeFile command = u16 action 2 + u16 size + RFLT |
| `archive.h/.cpp` — 'harc' container | `HotlineArchiveStruct.h` + `HotlineArchiveDecoder.cpp` | 98-byte header + per-file path head/path/file-rsvd/file-head + payload; `decompress_archive_entry()` inflates 'zlib' (system zlib) or passes 'raw ' through |
| `tracker.h/.cpp` — tracker messages | `TrackerServ.cpp:585-643, 1055-1170`; `HotlineTasks.cpp:5939-5959, 6057-6065` | UDP registration (type/port/userCount/flags/passID + 3 p-strings); 'HTRK' handshake v1/v2 (32-byte padded credentials); server-list messages (type 1, size, totalCount, count, entries with raw IP octets); lookup 4/5 |

### Audit correction (applied to audit/06 §6.4)

The audit printed the `dlFldrAction_*` mapping with SendFile/NextFile swapped. The verbatim
enum (`HotlineClientServerCommon.h`) and the server's dispatch — it waits for
`dlFldrAction_NextFile` before sending the next item — confirm **SendFile = 1, ResumeFile = 2,
NextFile = 3**. The modern `FolderDownloadAction` enum always had the correct values; the
audit text is fixed.

### Hardening divergences (documented)

- `DecompressArchiveEntry` caps `decompressedSize` at 64 MiB (`kMaxArchiveEntryDecompressedSize`)
  — the legacy decoder allocated straight from the attacker-visible field (audit/06 §10 hazard).
- `aw::DecodeError` gained the generic `wrong_format_tag` and `unsupported_version` values
  (shared by FILP/RFLT/harc/HTRK tag and version checks).

### Verification

- 24 new test cases (122 total): FILP header/info/fork goldens + round-trips + malformed
  sweeps; RFLT golden (100-byte data fork) + last-DATA-wins + tag/version/truncation errors;
  folder-item goldens (file/folder/multi-component) + verified 1/2/3 command mapping + resume
  command round-trip; harc raw + zlib goldens (python-oracle zlib payload) + header/entry/
  decompress error paths; tracker registration/handshake v1-v2/server-list/lookup goldens +
  shape errors.
- `hotline::protocol` now links system zlib (`find_package(ZLIB)`) for the harc payloads.
- gcc, clang, ASan/UBSan presets: 122/122 pass, warnings-as-errors clean.

---

## Phase 3b — Crypto completion

### Delivered

**`aw::crypto::Blowfish`** (`appwarrior/crypto/blowfish.h/.cpp`):

- Standard Schneier Blowfish core (hex-of-pi tables generated from the legacy
  `HLBlowfishData.h`; 16-round Feistel; key expansion wrapping key bytes) with independent
  encode/decode schedules (the login key schedule derives different keys per direction).
- `Ofb64` stream: 64-bit output feedback, **zero IV**, keystream bytes big-endian from the
  encrypted feedback block, byte-continuous across calls — exactly `HLBlowfish::OFB64`.
- Verified against the **complete Eric Young suite extracted from the legacy tree itself**
  (34 variable-key + 24 variable-length-key ECB vectors — the same suite the legacy
  `HLBlowfish::SelfTest` ran). OFB chaining verified structurally plus a 16-byte keystream
  cross-check against OpenSSL for the zero key.

**Oracle finding (recorded):** OpenSSL 3's legacy-provider Blowfish **deviates from the official
test vectors** (`enc -bf-ecb` fails vectors #2/#33/#34, e.g. key `0123456789abcdef` → `9713e3a4…`
instead of `24594688…`). It is not a trustworthy Blowfish oracle; the Eric Young suite from the
legacy tree is authoritative for Hotline compatibility.

**`hotline::protocol::auth::TransactionCipher<H>`** (`transaction_cipher.h`, header-only):
reproduces `_TNSendTran`/receive crypto mechanics exactly — header always stream-encoded under
the current key; flag 0 = whole body under current key; flag 1..32 = first **2 bytes** under the
current key, then `Perm*Key(flag)` and the remainder under the new key; OFB state persists across
transactions in both directions. `legacy_flag_quirk()` reproduces the historical sender's
0/2/7/13 re-roll distribution as a deterministic, tested helper.

**`hotline::protocol::auth::hope`** (`hope.h/.cpp`): the HOPE encrypted-login exchange —
stage-1 request (zero login/password bytes, 12-byte MacAlg list quirk preserved, 11-byte
CipherAlg list), server algorithm parsing (2-byte prefix + p-string name comparison, legacy
semantics), `login_digests<H>()` = HMAC(login/password, sessionKey), stage-2 digest login
(digests + server cipher field echoed + Vers 197), and a compatible server-side stage-2 reply
builder (the reference tree's server never sent one — client-only feature).

### Deliberate divergence (hardening)

- Legacy receive path with flag ≠ 0 and a 1-byte payload decoded **nothing** (its `s >= 2`
  guard skipped the old-key decode and `s > 2` skipped the rest — the byte stayed encrypted
  while the caller treated it as plaintext). The modern codec decodes the single byte under the
  old key. No valid peer is affected; documented here and in the code.

### Verification

- 17 new test cases (98 total): full Eric Young ECB suite, OFB keystream/continuity/reset/
  round-trip, flag-quirk deterministics, encrypted-transaction round-trips (all flag classes,
  both directions, stream continuity across transactions, 1-byte-payload fix), HOPE stage
  goldens + parse tests + digest goldens from an independent python implementation.
- gcc, clang, ASan/UBSan presets: 98/98 pass, warnings-as-errors clean.

## Phase 4c — Configurable AppWarrior library shape + AW_API exports

AppWarrior's linkage is now fully configurable on two independent axes, per project
decision (AGENTS.md amended):

- **`BUILD_SHARED_LIBS`** decides **shared vs static** (default ON = shared).
- **`BUILD_MONOLITHIC`** decides **monolithic vs modular** (default ON = one `appwarrior`
  library; OFF = per-component targets `appwarrior::core` / `appwarrior::crypto` /
  `appwarrior::testing`).
- Consumers always link the aggregate **`appwarrior::framework`** target, which resolves to
  the right set of libraries in either mode — `hotline::protocol` and the tests were switched
  to it, so no consumer code changes across configurations.

All four combinations are built and tested:

| Preset | BUILD_SHARED_LIBS | BUILD_MONOLITHIC | Artifacts |
|---|---|---|---|
| default (`gcc`/`clang`/`asan`) | ON | ON | `libappwarrior.so` |
| `static` | OFF | ON | `libappwarrior.a` |
| `modular` | ON | OFF | `libappwarrior_core.so` + `libappwarrior_crypto.so` |
| `static-modular` | OFF | OFF | `libappwarrior_core.a` + `libappwarrior_crypto.a` |
- **`AW_API`** (`appwarrior/export.h`): `__declspec(dllexport)` while building /
  `__declspec(dllimport)` while consuming on Windows (behind the `AW_BUILDING_LIBRARY`
  definition, set privately on the compiled targets when shared), empty elsewhere. Applied to
  the out-of-line public symbols (`aw::ivar` codec functions, `Md5::digest`, `Sha1::digest`,
  `Blowfish` + `Ofb64`); header-only inline facilities need no export annotation. The blanket
  `WINDOWS_EXPORT_ALL_SYMBOLS` was removed.

**Verification:** all four combinations plus the compiler/sanitizer presets pass 122/122
tests, warnings-as-errors clean.

## Phase 5 — Networking

### Delivered

**`aw::net`** (`src/appwarrior/net/`, POSIX backend; the Windows backend joins the platform
phase behind the same interface):

- `IpAddress` (IPv4 + port, strict text parse, network-order u32) — Hotline is an IPv4 wire
  protocol.
- RAII non-blocking `Socket` / `Listener` / `make_socket_pair()` with `std::expected<..., NetError>`
  throughout (would_block/closed/interrupted/refused/address_in_use/system; EINPROGRESS mapped —
  the classic non-blocking-connect case).
- `Poller` (poll(2)): register interests, readiness-driven `wait()` — no busy-waiting.

**`hotline::net::Connection`** (`src/hotline/net/`) — the UTransact replacement:

- Handshake in both roles: client sends 'TRTP' 'HOTL'/'HTXF' v1 sub2/3; server accepts 'TRTP'
  and the legacy 'NICK' alias, replies 'TRTP' + 0, rejects version != 1 with reason 1 and
  closes after flushing.
- Transaction framing with multi-part reassembly by (isReply, id), dispatching at totalSize
  with the last part's header.
- The historical receive policy verbatim: totalSize == 0, dataSize == 0, or either above the
  cap (2 MB framework / 512 KB server override) kills the connection — enforced AFTER the
  crypto header decode, exactly like the legacy order.
- `queue_keepalive()` reproduces the tree's real keepalive: transaction 500 with a **2-byte
  empty field list** body.
- Encrypted-transaction hooks (`choose_flag` / `encode` / `decode_header` / `decode_data`) that
  wrap `TransactionCipher<H>`; `TransactionCipher::decode` was split into header/data stages so
  the receive policy runs on decoded sizes.

**`hotline::net::Session`** — login/agreement state machine (fresh → awaiting_agreement →
active), extracted from `ProcessTran_Login`: login unscramble + ASCII lowercase + '
'→'-',
injected user lookup, password compared exactly as stored (received scrambled bytes vs stored
scrambled bytes — never unscrambled), success reply (Vers/CommunityBannerID/ServerName), error
replies with error 1 + ErrorText. The agreement/banner choreography itself lands with the
server core.

### Audit corrections / confirmations

- **Keepalive body:** audit/06 §11.1's "empty transaction (e.g. KeepConnectionAlive)" golden
  does not describe this tree's keepalive — `UFieldData::GetDataHandle` creates a 2-byte zero
  count, so the legacy client sends body `00 00` (dataSize 2). An actual empty body would be
  killed by the tree's own receive policy. audit/06 corrected.
- **SendErrorMsg format-string overload** (audit/06 §11.2): confirmed — it calls
  `SetSendError(1)` but never `SendData`, so the reply never goes out. The modern Session
  always sends the ErrorText reply (deliberate fix, documented).

### Verification

- 15 new tests (142 total across both suites): socketpair echo/would-block/close semantics,
  loopback listener/connect/accept/echo, poller edges; establish (TRTP/HTXF-v3/NICK/reject-
  reason-1), transaction round-trip, keepalive bytes, multi-part reassembly, zero/oversized
  kill policy, encrypted round-trip through TransactionCipher hooks (flag 5), and five session
  scenarios.
- 127 + 15 tests pass on gcc, clang, ASan/UBSan, and the static/modular/static-modular
  configurations.

## Phase 5a — Networking follow-ups (WinSock, IPv6, AW_API, role-split connections)

Four follow-ups per project decision:

1. **WinSock conditional compilation.** `aw::net` now carries parallel implementations in
   each translation unit: WinSock2 (WSAStartup runtime, `ioctlsocket(FIONBIO)`, `closesocket`,
   `WSAPoll`, WSA error mapping, `ws2_32` linked via CMake) vs POSIX (unchanged). The Windows
   half is compile-untested here (no Windows toolchain in this environment) and will be
   exercised by a Windows CI/build when the platform phase lands. `make_socket_pair()` is a
   loopback TCP pair on Windows (no socketpair there).
2. **`AW_API` on `IpAddress`** (class-level plus the out-of-line `from_text`/`to_text`
   declarations) — the class previously escaped the export annotations.
3. **IPv6.** `IpAddress` now carries a family (ipv4/ipv6), 16 bytes, and a scope id: strict
   parsing of `a.b.c.d[:port]`, `[v6]:port`, and bare IPv6 (single "::" compression, 4-digit
   hex groups, bracketed ports required for v6); text output with longest-zero-run
   compression. `Socket`/`Listener` create per-family sockets; IPv6 listeners are dual-stack
   (`IPV6_V6ONLY=0`) so IPv4-only Hotline traffic works over IPv6 hosts. The Hotline WIRE
   formats remain IPv4 (tracker octets etc.).
4. **Role-split connections.** `hotline::net::Connection` was split into
   **`ClientConnection`** (client-only API: `start(socket, sub_protocol, sub_version)`) and
   **`ServerConnection`** (server-only API: `start(socket)`, remote version getters). The
   enforcement is **structural**: a shared `detail::ConnectionBase` carries the framing
   machinery, while the role-specific `detail::ClientCore` (only a client start) and
   `detail::ServerCore` (only a server start + remote getters) live behind the public
   wrappers — there is no class anywhere with both entry points, so the client cannot call
   the server's start and vice versa, even in principle.

**Verification:** 2 new tests (144 total); IPv6 parse/text round-trips and a v6 loopback
listener test join the suite; 129 + 15 tests pass on gcc, clang, ASan/UBSan, static, modular,
and static-modular configurations.

## Phase 5b — Component gating and UDP transport

Two project decisions delivered together:

1. **Component gating (`BUILD_CLIENT` / `BUILD_SERVER` / `BUILD_TRACKER`).** The client
   and server role code is no longer merely *shaped* differently — it is *compiled*
   conditionally. `hotline::net` publishes `HOTLINE_BUILD_CLIENT` / `HOTLINE_BUILD_SERVER`
   as PUBLIC definitions (defaulted to 0 in the headers when undefined):
   `ClientConnection` + `detail::ClientCore` exist only in client-enabled builds;
   `ServerConnection` + `detail::ServerCore` + the login `Session` only in server-enabled
   builds. A client-only binary contains no server start path and no login state machine;
   a server-only binary contains no client start path. `BUILD_TRACKER` gates the future
   tracker application; the tracker WIRE codecs remain always-available (they are pure
   protocol). New presets `client-only` and `server-only` verify both gated
   configurations; the net test suite is itself role-gated (both-role tests require both
   switches, plus dedicated client-role and server-role cases), so every configuration
   compiles and runs a meaningful subset.
2. **UDP transport in `aw::net`.** `Socket::create_udp(family)` (non-blocking datagram
   sockets, dual-stack IPv6), `Socket::bind(address)` (port 0 → ephemeral, query via
   `local_address()`), `Socket::send_to(destination, buffer)`, and
   `Socket::receive_from(buffer) -> expected<Datagram, NetError>` with
   `Datagram { bytes_received, from }`. Zero-length datagrams are first-class: they
   succeed with `bytes_received == 0` and never fold into the stream-only
   `connection_closed` signal (empty-buffer *arguments* are still `invalid_argument`).
   WinSock `sendto`/`recvfrom` alongside POSIX (with `MSG_NOSIGNAL`), and
   `EAFNOSUPPORT` maps to `invalid_argument` on both branches.

**Verification:** 4 new UDP tests (loopback exchange with sender address both ways,
zero-length datagram, would-block/closed-socket error mapping, IPv6 loopback exchange when
available) — 133 + 16 tests pass on gcc, clang, ASan/UBSan, static, modular,
static-modular, client-only, and server-only.

### Recommended next phase

**Phase 6 — Server core.** With the transport/connection/session stack complete: the four
listeners at basePort/+1/+2/+3, the 54-handler dispatch table, user/news databases,
agreement/banner sequence, flood protection, and path-safety hardening.
