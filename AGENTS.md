# AGENTS.md — Hotline / AppWarrior C++23 Modernization

## Mission

This repository is a proof-of-concept modernization workspace for **Hotline** and its application framework, **AppWarrior**.

The historical source originates from:

`https://github.com/Schala/Gloarbline`

The historical repository is the primary behavioral and archaeological reference for the modernization.

Throughout new code, documentation, architecture, reports, and discussion, refer to the application and protocol as **Hotline**.

The historical repository name is provenance only.

The project has two closely related modernization targets:

1. **AppWarrior** — preserve and modernize it as a lightweight native C++23 cross-platform application/UI framework.
2. **Hotline** — modernize the client, server, tracker, and related applications while continuing to use AppWarrior.

The objective is not merely to make old source compile.

The objective is:

> Reconstruct Hotline and AppWarrior as clean, maintainable, native, portable C++23 software while preserving useful architecture, Hotline protocol compatibility, application behavior, and the lightweight native character of the original software.

---

# Existing Audit

A repository-wide archaeological audit has already been performed.

Before substantial implementation work, read:

`HOTLINE_MODERNIZATION_REPORT.md`

Supporting subsystem reports are under:

`audit/`

Treat the consolidated report as architectural orientation and the individual audit reports as supporting evidence.

## Do not repeat the complete repository audit.

Perform additional archaeology only when necessary for the currently assigned subsystem or implementation task.

The existing audit is guidance, not immutable law. If direct source analysis disproves an audit conclusion, trust verified source behavior and update the documentation accordingly.

---

# Architectural Correction Regarding AppWarrior

**AppWarrior is being ported and modernized.**

Do not dismantle AppWarrior wholesale.

Do not migrate Hotline away from AppWarrior.

The following applications should continue to use modern AppWarrior where appropriate:

- Hotline client;
- Hotline server;
- Hotline tracker;
- related Hotline utilities where AppWarrior provides useful infrastructure.

However:

> Preserving AppWarrior does not mean preserving every historical AppWarrior implementation detail.

AppWarrior's meaningful architecture, UI concepts, event model, application model, and useful abstractions should survive.

Facilities that merely reimplemented functionality now provided better by C++23 should normally be replaced internally by the standard library.

Facilities tied to obsolete operating systems should be reimplemented through modern native platform backends.

Facilities with no remaining purpose should be removed.

---

# AppWarrior's Modern Role

Modern AppWarrior should become a **lightweight native cross-platform C++23 application framework**.

It should provide useful shared infrastructure for Hotline without requiring applications to directly implement every platform's UI APIs.

Conceptually:

```text
                    AppWarrior C++23
                         │
          ┌──────────────┼──────────────┐
          │              │              │
        macOS           Linux         Windows
          │              │              │
   Objective-C++     Wayland/X11      native
       Cocoa          backends        Win32/
                                    Windows APIs
```

Exact backend technologies should be selected according to actual requirements and contemporary platform capabilities.

The public AppWarrior interface should remain primarily portable C++.

Platform-specific implementation belongs behind clearly defined backend boundaries.

---

# Native UI Requirement

AppWarrior must use **native/lightweight desktop technology**.

Do not replace the UI with:

- Electron;
- Chromium Embedded Framework as the application UI;
- browser-hosted UI;
- an embedded web application;
- another architecture requiring an entire browser runtime merely to draw the application.

Memory efficiency and native integration matter.

Hotline should remain a lightweight desktop application.

---

# macOS Backend

The macOS AppWarrior backend should use contemporary native macOS APIs.

Use **Cocoa/AppKit**, with **Objective-C++ (`.mm`)** where C++ and Objective-C integration is required.

Keep Objective-C++ at the platform boundary.

Portable AppWarrior code should remain C++23.

Conceptually:

```text
Hotline
   ↓
portable AppWarrior C++
   ↓
AppWarrior macOS backend
   ↓
Objective-C++
   ↓
Cocoa / AppKit
```

Do not expose Cocoa types throughout portable application code unnecessarily.

Do not restore Carbon, classic Mac Toolbox APIs, QuickDraw, or classic Mac UI infrastructure.

---

# Linux Backend

Provide native Linux desktop backends.

Support contemporary Linux display environments with particular attention to:

- **Wayland**
- **X11**

Do not assume X11 is the permanent universal Linux display server.

Do not assume Wayland completely replaces every X11 use case.

Structure the backend boundary so AppWarrior can support both without contaminating application code with display-server-specific logic.

Where practical, share Linux implementation code above the Wayland/X11 boundary.

Avoid pulling in enormous application frameworks merely to avoid implementing the AppWarrior backend.

---

# Windows Backend

Provide a native Windows backend suitable for contemporary **Windows 11**.

Use appropriate native Windows desktop facilities.

Win32 remains acceptable where it provides the required functionality.

Modern Windows facilities such as Direct2D, DirectWrite, or related APIs may be used where they provide clear benefits.

Do not select heavyweight UI infrastructure merely because it is newer.

Keep Windows-specific types and implementation details behind the AppWarrior backend boundary.

---

# Platform Boundary

Avoid architecture such as:

```cpp
#ifdef _WIN32
// giant implementation
#elif __APPLE__
// another giant implementation
#else
// Linux
#endif
```

throughout generic source.

Prefer explicit backend implementation units.

For example:

```text
AppWarrior/
    include/
    src/
        core/
        ui/
        platform/
            macos/
            linux/
                wayland/
                x11/
            windows/
```

This layout is illustrative, not mandatory.

Platform selection should happen at well-defined architectural boundaries.

---

# Preserve AppWarrior Concepts, Not Obsolete Internals

When modernizing an AppWarrior class, ask:

1. What service does this class provide?
2. Is that service still useful to Hotline?
3. Is it part of AppWarrior's meaningful public abstraction?
4. Can its implementation become dramatically simpler using C++23?
5. Does it represent a native-platform concept that needs a modern backend?
6. Is it merely recreating functionality already supplied by the STL?

The answer determines whether to:

- preserve and modernize the abstraction;
- preserve the abstraction but replace its internals;
- merge it with another AppWarrior abstraction;
- replace callers with an STL facility;
- remove it entirely.

---

# C++ Standard

All new production C++ targets:

**C++23**

Use contemporary idiomatic C++.

Prefer:

- RAII;
- value semantics;
- explicit ownership;
- strong types;
- standard containers;
- standard algorithms;
- standard concurrency;
- standard filesystem facilities;
- standard time facilities;
- concepts where meaningful;
- narrow interfaces;
- deterministic resource lifetime.

Do not write "C with classes."

Do not introduce modern features merely for novelty.

---

# Utility Classes and Namespaces

Do not preserve or create classes whose only purpose is grouping unrelated static functions.

When no object state or meaningful class abstraction exists, prefer namespace-scope functions.

For example:

```cpp
namespace appwarrior::math {

constexpr auto some_operation(...) noexcept -> ...;

}
```

Classes should represent meaningful:

- objects;
- resources;
- state;
- UI elements;
- domain concepts;
- polymorphic behavior;
- ownership.

This rule does **not** prohibit static class members where they genuinely belong to a class abstraction.

---

# Naming

Modernize historical naming where doing so improves clarity.

Remove prefixes that existed primarily to prevent global namespace collisions when namespaces/scoped enums now solve that problem.

Do not mechanically rename every AppWarrior class merely because its name is old.

Preserving recognizable framework concepts is valuable when those concepts remain valid.

Use judgment.

---

# Enums

Convert appropriate unscoped enums to:

```cpp
enum class
```

Remove redundant value prefixes made unnecessary by enum scoping.

Specify underlying types where representation matters:

```cpp
enum class TransactionType : std::uint16_t {
    ...
};
```

Never rely accidentally on compiler enum layout for protocol serialization.

---

# Ownership

## Raw owning pointers are prohibited in new code.

Prefer, in order:

1. direct value ownership;
2. `std::unique_ptr`;
3. `std::shared_ptr` only when lifetime is genuinely shared.

Do not mechanically convert raw pointers to `std::shared_ptr`.

Raw pointers remain valid for clearly non-owning nullable references.

References are appropriate for non-null borrowed objects.

Ownership should be understandable from an interface without archaeological investigation.

---

# Arrays and Contiguous Storage

Choose representations according to semantics.

Use:

- `std::array<T, N>` — fixed-size owned storage;
- `std::vector<T>` — dynamically sized contiguous ownership;
- `std::span<T>` — borrowed contiguous storage;
- `std::string` — owned text;
- `std::string_view` — borrowed text;
- `std::byte` — opaque binary data.

Prefer `std::span` over raw pointer + count interfaces.

Do not blindly turn every historical array into `std::array`.

---

# Strings

Replace raw C-string application architecture with:

```cpp
std::string
std::string_view
```

according to ownership.

Legacy uses of:

```text
strcpy
strncpy
strcat
strncat
strcmp
strncmp
strtok
```

and AppWarrior equivalents should normally disappear as surrounding APIs become type-safe.

Protocol-defined fixed-size strings must be handled explicitly and safely.

---

# ANSI.h

`ANSI.h` is obsolete compatibility infrastructure.

It should not survive as a modern AppWarrior compatibility layer.

Migrate its users to C++ standard-library facilities.

Prefer C++ facilities over C equivalents where practical.

Do not create:

```text
ModernANSI
ANSICompat
ANSI23
```

The functionality survives through the standard library.

The abstraction does not need to.

---

# Standard Library Replacement Policy

When AppWarrior contains a custom implementation of something now adequately represented by the C++ standard library, prefer the standard library.

However:

> Do not mechanically map classes by name.

Analyze semantics and call sites.

AppWarrior may retain thin abstractions where they add genuine framework semantics beyond merely duplicating STL behavior.

---

# CBoolArray

Analyze usage.

Potential replacements include:

```cpp
std::vector<bool>
std::vector<std::uint8_t>
std::bitset<N>
```

depending on semantics.

Remember that `std::vector<bool>` is a specialized packed representation with proxy references.

---

# CLinkedList

Do not automatically replace with `std::list`.

Prefer `std::vector` where contiguous storage works.

Consider:

```cpp
std::vector
std::deque
std::list
```

according to actual mutation, stability, and traversal requirements.

---

# CPtrList

Likely replacements include:

```cpp
std::vector<T>
std::vector<std::unique_ptr<T>>
```

depending on ownership and polymorphism.

Prefer values when possible.

---

# CPtrTree

Do not assume an STL replacement from the class name.

Analyze what tree semantics it actually provides.

Possible replacements include:

```cpp
std::map
std::unordered_map
std::set
std::unordered_set
```

or an explicit hierarchical tree structure.

If AppWarrior provides meaningful tree-specific behavior not represented by an STL container, a modern AppWarrior tree abstraction may remain appropriate.

Do not retain it merely because it existed historically.

---

# UIDVarArray

Analyze actual semantics.

If it merely compensates for historical compiler/container limitations, replace it.

If it provides meaningful behavior absent from standard containers, modernize only that meaningful behavior.

---

# UBitString

Analyze whether it represents:

- a fixed bit field;
- dynamically sized bits;
- protocol bits;
- application flags.

Potential facilities include:

```cpp
std::bitset<N>
std::vector<bool>
```

or a small purpose-built modern type if dynamic bit semantics genuinely require one.

---

# Integer Types

Replace historical primitive aliases with `<cstdint>` where representation matters:

```cpp
std::int8_t
std::uint8_t
std::int16_t
std::uint16_t
std::int32_t
std::uint32_t
std::int64_t
std::uint64_t
```

Use `std::size_t` and semantic types where fixed width is unnecessary.

Protocol fields must have explicit widths.

---

# Constants

Replace object-like `#define` constants with typed constants.

At namespace scope prefer:

```cpp
constexpr
inline constexpr
```

Use:

```cpp
static constexpr
```

for actual class members.

Do not create utility classes solely to hold constants.

---

# Function Macros

Replace function-like macros with:

- normal functions;
- `constexpr` functions;
- templates;
- concepts;
- standard algorithms;

where appropriate.

Keep macros only when preprocessing itself is genuinely required.

---

# Type Aliases

Prefer:

```cpp
using
```

over:

```cpp
typedef
```

when aliases remain meaningful.

Remove aliases that merely obscure already-clear standard types.

Consider strong domain types when unrelated values could otherwise be accidentally mixed.

---

# Concepts

C++ concepts are part of the modernization strategy.

Include:

```cpp
#include <concepts>
```

where standard concepts are appropriate.

During modernization, identify generic interfaces whose requirements were historically:

- implicit;
- documented only in comments;
- enforced through inheritance;
- enforced through macros;
- assumed by templates;
- represented by old AppWarrior conventions.

Express those requirements using standard concepts when possible.

Examples include:

```cpp
std::integral
std::signed_integral
std::unsigned_integral
std::floating_point
std::derived_from
std::convertible_to
std::same_as
std::invocable
std::predicate
```

and related standard concepts.

Where AppWarrior or Hotline has a genuine domain-specific generic requirement not adequately represented by `<concepts>`, define a **small, meaningful custom concept**.

For example, a generic serialization operation might reasonably require a concept describing supported protocol scalar types.

Do not create concepts simply to make ordinary concrete functions look sophisticated.

Do not create a concept hierarchy when a straightforward function or class interface is clearer.

Concepts should make compile-time contracts explicit.

---

# Memory Operations

Do not mechanically replace `memcpy` with `std::copy`.

For logical element operations prefer:

```cpp
std::copy
std::copy_n
std::ranges::copy
```

For exact byte/object-representation copying, `std::memcpy` may remain correct.

Use move semantics where values or ownership are being transferred.

---

# Casts

Do not introduce C-style casts.

Use the narrowest appropriate C++ cast:

```cpp
static_cast
dynamic_cast
const_cast
reinterpret_cast
```

Prefer better types that eliminate casts.

Use:

```cpp
std::bit_cast
```

for legitimate representation conversion between appropriately sized trivially copyable types.

`std::bit_cast` is not a universal replacement for `reinterpret_cast`.

---

# Unions

Use `std::variant` when a union semantically represents one active alternative from several types.

Do not mechanically replace unions involved in:

- external binary layouts;
- protocol formats;
- platform APIs;
- low-level storage;
- object representation.

Replace type-punning unions with defined modern mechanisms.

---

# Files and Paths

Prefer:

```cpp
std::filesystem::path
std::ifstream
std::ofstream
std::istream
std::ostream
```

where appropriate.

Replace ordinary `FILE*` application architecture with modern C++ I/O.

Use explicit byte-oriented I/O where streams are not the clearest solution.

---

# MoreFiles

The obsolete **MoreFiles** library has intentionally been removed.

Do not restore it.

Replace its functionality with:

```cpp
std::filesystem
```

or narrow platform-specific implementation where standard facilities are insufficient.

---

# Classic Mac Handles

Classic Mac `Handle` types represent obsolete relocatable Memory Manager allocations.

Do not recreate that memory model merely to preserve source structure.

Migrate individual storage uses to:

- values;
- `std::vector`;
- `std::array`;
- `std::string`;
- `std::unique_ptr`;
- `std::span`;
- other appropriate RAII types.

AppWarrior may retain a higher-level resource abstraction if that abstraction remains useful, but it must not require classic Mac movable memory semantics.

---

# StHandleLocker

Determine its historical semantics before modifying it.

If `StHandleLocker` exists to invoke classic Mac `HLock`/`HUnlock`, it is **not a threading lock**.

Do not replace that behavior with:

```cpp
std::mutex
std::atomic
```

The movable-memory locking concept should disappear when movable Handles disappear.

Use synchronization only for actual concurrency.

---

# Threading

For real concurrent behavior use appropriate facilities such as:

```cpp
std::jthread
std::stop_token
std::mutex
std::scoped_lock
std::lock_guard
std::unique_lock
std::condition_variable
std::atomic
```

Prefer deterministic shutdown and RAII.

Do not introduce locks or atomics without a demonstrated concurrency requirement.

---

# Time

Replace historical timing implementation such as `UTimer` with `<chrono>` internally.

AppWarrior may retain a useful timer abstraction where timers participate in its event/application model.

The abstraction should be backed by:

```cpp
std::chrono
```

and/or appropriate native event-loop timers.

Use explicit durations.

Prefer:

```cpp
std::chrono::steady_clock
```

for elapsed time.

Avoid implicit integer time units.

---

# Math and Bit Operations

Replace obsolete generic `UMath` functionality with standard facilities where possible:

```text
<cmath>
<bit>
<numbers>
<numeric>
<algorithm>
```

Use:

```cpp
std::byteswap
std::endian
```

where appropriate.

If `UMath` contains meaningful AppWarrior-specific functionality not represented by the standard library, preserve only that useful portion in a modern form.

---

# Algorithms and Ranges

Use:

```cpp
<algorithm>
<ranges>
```

where they improve clarity.

Prefer standard algorithms over hand-written implementations of standard operations.

Do not turn simple readable loops into complicated ranges pipelines merely for novelty.

---

# Randomness

Replace ordinary:

```cpp
rand()
srand()
```

with `<random>`.

Do not alter protocol-defined deterministic behavior merely because its historical implementation uses old random-number facilities.

Security-sensitive randomness requires an appropriate secure source.

---

# Function Pointers and Callables

Function pointers remain valid modern C++.

Do not mechanically convert every function pointer to `std::function`.

Choose among:

- ordinary function pointers;
- lambdas;
- templates;
- constrained callables;
- `std::function`;

based on semantics.

Use `std::function` when runtime type-erased callable storage is genuinely required.

Use concepts where compile-time callable requirements are appropriate.

---

# Compile-Time Facilities

Use:

```cpp
constexpr
```

where natural.

Use:

```cpp
static_assert
```

for meaningful compile-time assumptions.

Potential uses include:

- protocol constants;
- fixed tables;
- masks;
- FourCC values;
- integer widths;
- representation assumptions.

Use `consteval` only where compile-time evaluation is genuinely mandatory.

---

# Error Handling

Replace ambiguous sentinel values, magic integer errors, and global error-state architecture with explicit modern error handling.

Consider:

```cpp
std::expected<T, Error>
std::optional<T>
```

where appropriate.

**Amended (project decision, post-Phase 4):** production libraries (AppWarrior and
Hotline) use `std::expected` / `std::unexpected` for ALL recoverable errors — decode *and*
encode paths — and never throw for error handling. The only exceptions remaining are the test
harness's assertion control flow (`aw::test::CheckFailed`) and standard-library facilities.

Protocol errors should be structured and informative.

AppWarrior platform backends should translate platform-specific failures into portable framework-level errors where practical.

---

# Binary Data

Prefer:

```cpp
std::byte
std::span<std::byte>
std::span<const std::byte>
std::vector<std::byte>
```

for opaque binary data.

Do not treat arbitrary binary storage as text.

---

# Hotline Protocol

Hotline wire compatibility is a primary invariant.

Do not assume native C++ object layout equals wire layout.

Avoid packet parsing such as:

```cpp
auto* packet =
    reinterpret_cast<const Packet*>(buffer);
```

Prefer explicit codecs:

```cpp
std::expected<Packet, DecodeError>
decode_packet(std::span<const std::byte> bytes);
```

and:

```cpp
std::vector<std::byte>
encode_packet(const Packet& packet);
```

Explicitly handle:

- widths;
- byte order;
- lengths;
- bounds;
- malformed data;
- optional fields;
- protocol quirks.

Treat all network input as untrusted.

---

# Endianness

Hotline's history includes big-endian Macintosh systems.

Audit historical byte-swapping logic carefully.

Distinguish:

- Hotline/network byte order;
- file-format byte order;
- host byte order;
- obsolete platform conversions.

Centralize byte-order handling.

Do not delete historical endian logic until its purpose is proven obsolete.

---

# Networking

Separate transport from protocol semantics.

Conceptually:

```text
native socket
     ↓
AppWarrior/network transport
     ↓
connection
     ↓
Hotline framing
     ↓
Hotline codec
     ↓
session/state
     ↓
application
```

AppWarrior may provide cross-platform transport facilities where this remains useful.

Those facilities should use modern native networking and RAII rather than preserving obsolete socket wrappers internally.

Do not entangle UI widgets with packet serialization.

---

# QuickTime

Classic QuickTime APIs are obsolete.

Do not restore them.

Analyze each QuickTime-related AppWarrior facility to determine what abstraction it exposed.

The **QuickTime implementation** should be discarded.

The **useful AppWarrior media abstraction** may survive if Hotline or AppWarrior still benefits from it.

Where retained, reimplement it using an appropriate contemporary media backend.

If a feature has no useful modern role, document its removal.

Do not create QuickTime compatibility emulation.

---

# Image Handling

AppWarrior may continue to provide image abstractions where they are useful to its UI architecture.

Do not preserve obsolete custom image decoders merely because they exist.

Prefer appropriate modern/native decoding facilities unless AppWarrior requires a small portable decoder for a justified reason.

Separate:

- image representation;
- decoding;
- platform drawing.

Do not tightly couple image objects to one OS backend.

---

# Netscape / Mozilla-Era Code

Audit historical Netscape/Mozilla-derived code according to functionality.

Do not preserve obsolete browser/plugin architecture.

Useful functionality such as:

- URL handling;
- HTTP;
- MIME;
- encoding;

may survive through modern implementations.

The abstraction may remain if it provides useful AppWarrior functionality.

The obsolete Netscape implementation should not.

---

# AppWarrior UI Model

Preserve and modernize meaningful AppWarrior UI concepts.

Likely concepts include things such as:

- application;
- window;
- view;
- controls;
- buttons;
- check boxes;
- edit fields;
- dialogs;
- events;
- menus;
- images;
- drawing;
- timers.

Do not mechanically redesign these concepts merely because the historical implementation is old.

Instead:

1. identify the portable semantic contract;
2. clean up the C++ interface;
3. implement the contract using native platform backends.

AppWarrior should provide Hotline with a common UI model while retaining native implementation underneath.

---

# Native Widgets vs Custom Drawing

Determine the appropriate strategy per control.

Some AppWarrior controls may map naturally to native platform widgets.

Others may be more consistent and portable when drawn/managed by AppWarrior.

Do not impose one strategy universally before understanding the historical behavior and desired modern appearance.

Prioritize:

- lightweight implementation;
- native integration;
- predictable behavior;
- accessibility where practical;
- high-DPI support;
- modern input behavior.

---

# Event Loop

AppWarrior's modern architecture must account for native event loops.

Do not busy-wait.

Do not emulate every platform using a polling loop merely for portability.

Define a portable AppWarrior event abstraction and integrate it properly with:

- Cocoa's event/application model;
- Wayland/X11 event handling;
- Windows message dispatch.

Networking and background work must interact safely with the UI thread.

---

# Client, Server, and Tracker

The Hotline:

- client;
- server;
- tracker;

remain AppWarrior-based applications.

Do not migrate them away from AppWarrior as part of modernization.

However, only use AppWarrior facilities where framework-level abstraction makes sense.

The server should not instantiate UI infrastructure merely because AppWarrior historically bundled unrelated facilities together.

AppWarrior itself should have sufficiently modular components that non-GUI applications can use its useful core/network/platform facilities without paying for the GUI stack.

---

# AppWarrior Modularity

> **Amended (project decision):** AppWarrior's library shape is **configurable**: by default
> it ships as **one monolithic SHARED library** (CMake target `appwarrior`, alias
> `appwarrior::appwarrior`). `BUILD_MONOLITHIC=OFF` builds per-component targets
> (`appwarrior::core`, `appwarrior::crypto`, `appwarrior::testing`), and the standard
> `BUILD_SHARED_LIBS=OFF` selects static libraries. Consumers always link the aggregate
> target **`appwarrior::framework`**, which resolves correctly in either mode. Windows DLL
> exports use the `AW_API` macro (`appwarrior/export.h` — `__declspec(dllexport/dllimport)`
> behind `AW_BUILDING_LIBRARY`); no blanket `WINDOWS_EXPORT_ALL_SYMBOLS`.

The *component organization* survives as a source-level structure, not a linkage structure:

```text
src/appwarrior/
    core/        aw::endian, aw::bits, aw::align, aw::ivar, aw::guid, aw::DecodeError
    crypto/      aw::crypto (MD5, SHA-1, HMAC, Blowfish)
    testing/     aw::test (test registry, assertion macros, runner)
    ui/ …        (future)
    platform/ …  (future backend implementation units)
```

Each component keeps its own directory and namespace, so the conceptual decomposition is
preserved at the source level regardless of the chosen linkage shape. (Note: a shared library
links ALL of its objects, so the static-linker argument applies only in static builds; keep
platform backends behind the header/namespace boundaries so the code organization remains
clean in every configuration.) All four combinations of the two options are supported and
tested; do not introduce additional ad-hoc library targets — use the component list above.

---

# Historical Code

Historical implementations such as `ServerOLD`, duplicate source trees, and extinct platform branches are evidence.

Inspect them where they clarify behavior.

Do not compile obsolete implementations into the modern product merely because they exist.

Do not delete historical evidence before understanding whether it contains unique behavior.

---

# Security

Audit touched code for:

- unchecked lengths;
- buffer overflows;
- integer overflow;
- path traversal;
- use-after-free;
- dangling pointers;
- double free;
- malformed packets;
- format-string vulnerabilities;
- race conditions;
- unsafe temporary buffers;
- trust-boundary violations.

Do not silently alter protocol-required authentication in ways that destroy Hotline compatibility.

Isolate and document historically weak security mechanisms.

---

# Suspicious Legacy Behavior

The audit discovered unusual server behavior that may represent:

- debugging commands;
- administrative facilities;
- jokes;
- undocumented features;
- intentional backdoors.

Do not automatically preserve or remove such behavior.

For each suspicious behavior:

1. verify it in source;
2. trace activation conditions;
3. determine required privileges;
4. determine effects;
5. determine whether interoperability requires it;
6. document the conclusion.

Undocumented security-sensitive behavior should not survive by accident.

---

# Formatting and Logging

Prefer safe modern facilities such as:

```cpp
std::format
std::print
```

where appropriate and supported.

Remove unsafe `sprintf`-style formatting.

Modernize logging into a coherent facility rather than scattered debug output.

Do not log credentials or secrets.

---

# Build System

Use a clean contemporary build system.

Prefer:

**CMake + Ninja**

unless repository requirements demonstrate a better alternative.

Support building appropriate targets independently.

For example:

```text
appwarrior::framework  (aggregate of the AppWarrior library/libraries — see
                        "AppWarrior Modularity" for BUILD_MONOLITHIC /
                        BUILD_SHARED_LIBS)
hotline::protocol      (protocol codec library)
Hotline client
Hotline server
Hotline tracker
tests
```

Configure Objective-C++ only for macOS backend implementation that requires it.

Do not force non-macOS builds through Objective-C++.

---

# Testing

Tests are mandatory.

Prioritize AppWarrior tests for:

- containers/utilities retained by the framework;
- event behavior;
- ownership;
- geometry;
- platform-independent UI state;
- timers;
- networking abstractions.

Prioritize Hotline tests for:

- packet encoding;
- packet decoding;
- malformed packets;
- endian handling;
- authentication;
- transaction framing;
- file-transfer metadata;
- tracker behavior;
- state transitions;
- regression cases.

Use historical packet captures or known protocol vectors where available.

Platform backend behavior should be tested at appropriate integration boundaries.

---

# Sanitizers and Analysis

Keep appropriate targets practical to run with:

- AddressSanitizer;
- UndefinedBehaviorSanitizer;
- ThreadSanitizer where relevant.

Use compiler diagnostics and static analysis.

Do not suppress legacy warnings merely to obtain a clean build.

Understand and fix them.

---

# Documentation

Document architectural decisions.

In particular document:

- AppWarrior APIs preserved;
- AppWarrior APIs redesigned;
- STL replacements;
- removed compatibility infrastructure;
- platform backend architecture;
- QuickTime replacement/removal;
- Netscape replacement/removal;
- Hotline protocol quirks;
- security-sensitive compatibility;
- unresolved archaeology.

When historical behavior is deliberately removed, say so.

---

# Comments

Preserve comments containing important:

- Hotline protocol knowledge;
- AppWarrior semantics;
- compatibility quirks;
- binary-format information;
- reverse-engineered behavior;
- non-obvious invariants.

Remove comments that merely describe obsolete implementation after that implementation disappears.

New comments should primarily explain **why**.

---

# No Blind Substitution

Never apply transformations such as:

```text
CLinkedList → std::list
CPtrTree → std::map
Handle → std::unique_ptr
function pointer → std::function
union → std::variant
memcpy → std::copy
reinterpret_cast → std::bit_cast
```

without understanding semantics.

These are possible outcomes, not rules.

---

# No Compatibility Theater

Do not preserve obsolete implementation shapes merely to minimize changes.

Avoid constructs such as:

```text
ModernHandle
ModernANSI
CarbonCompat
QuickTimeCompat
ClassicMacMemoryManager
```

unless an explicitly temporary migration adapter is genuinely necessary.

Temporary adapters must be:

- narrow;
- documented;
- transitional;
- removable.

---

# But Do Preserve Useful Framework Abstractions

The prohibition against compatibility theater does **not** mean AppWarrior should collapse into raw STL and OS APIs.

If an abstraction provides meaningful portable framework semantics, retain and modernize it.

For example, a portable:

```cpp
appwarrior::Window
```

implemented by:

```text
NSWindow
Wayland/X11 window infrastructure
HWND
```

is a legitimate framework abstraction.

A wrapper whose sole purpose is making `std::vector` look like a 1998 pointer list is not.

This distinction is fundamental.

---

# Prefer Values

Historical pointer-heavy code may reflect limitations of old containers and compilers.

Prefer values where practical.

Use dynamic allocation when object identity, polymorphism, stable lifetime, or other real requirements justify it.

---

# Avoid Gratuitous Shared Ownership

`std::shared_ptr` requires a real shared-lifetime justification.

Prefer `std::unique_ptr` plus borrowed references/pointers where possible.

Do not replace ambiguous legacy ownership with universal reference counting.

---

# Performance and Footprint

Hotline and AppWarrior should remain lightweight.

Avoid architectures that impose large runtime footprints without corresponding value.

Be mindful of:

- unnecessary allocations;
- cache-unfriendly containers;
- excessive type erasure;
- unnecessary shared ownership;
- unnecessary background threads;
- heavyweight UI/runtime dependencies.

Do not prematurely micro-optimize.

Use appropriate algorithms and data structures first.

---

# Avoid Overengineering

Do not replace historical overengineering with fashionable modern overengineering.

Avoid unnecessary:

- dependency-injection frameworks;
- service locators;
- abstract factories;
- event buses;
- visitor hierarchies;
- giant template frameworks;
- excessive interfaces;
- unnecessary runtime polymorphism;
- elaborate concept hierarchies.

Use abstractions because AppWarrior or Hotline needs them, not because modern C++ permits them.

---

# Scope Discipline

For each implementation assignment:

1. read `HOTLINE_MODERNIZATION_REPORT.md`;
2. read the relevant supporting audit report;
3. inspect relevant AppWarrior/Hotline source;
4. inspect important callers;
5. understand behavior;
6. establish or update tests;
7. implement the scoped modernization;
8. build;
9. test;
10. fix regressions;
11. update documentation when architectural knowledge changes.

Do not initiate another complete repository audit.

Do not spawn a large swarm of subagents unless the assigned task genuinely has enough independent work to justify it.

Focused investigation is preferred.

---

# Reviewability

Keep changes coherent and reviewable.

Avoid combining unrelated:

- formatting;
- renaming;
- architecture;
- behavior;
- platform backend work;

into enormous undifferentiated changes.

A reviewer should be able to answer:

- what changed?
- why?
- what old functionality does it replace?
- what AppWarrior contract remains?
- how was behavior verified?

---

# When Uncertain

Do not guess.

Consult:

1. `HOTLINE_MODERNIZATION_REPORT.md`;
2. relevant `audit/*.md`;
3. current proof-of-concept code;
4. historical source;
5. callers/callees;
6. corresponding client/server/tracker implementation;
7. tests;
8. known Hotline protocol evidence.

If uncertainty remains, document it explicitly.

---

# Priority Order

When requirements conflict, prioritize:

1. **Hotline protocol interoperability**
2. **Correctness**
3. **Security**
4. **AppWarrior's useful portable architecture**
5. **Native lightweight UI behavior**
6. **Explicit ownership and lifetime**
7. **Maintainability**
8. **Portability**
9. **Performance and memory footprint**
10. **Modern stylistic elegance**

Do not sacrifice behavior merely to produce fashionable C++.

---

# Definition of Done

A modernization task is not complete merely because it compiles.

It should:

- preserve required Hotline behavior;
- preserve or improve meaningful AppWarrior behavior;
- have explicit ownership;
- replace obsolete internals rather than disguising them;
- use appropriate C++23 facilities;
- use standard or custom concepts where generic contracts warrant them;
- retain native lightweight platform integration;
- include or update relevant tests;
- compile cleanly;
- update documentation when new architectural facts are discovered.

The long-term result should feel like:

> **Hotline and AppWarrior implemented properly for 2026 in C++23, retaining the useful framework and application design while replacing the obsolete machinery underneath it.**

Not:

> **A 2007 codebase wearing a C++23 trench coat.**

And not:

> **A native lightweight application replaced by a web browser pretending to be a desktop program.**