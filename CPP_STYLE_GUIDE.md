# C++ style and design guide

This project uses C++17 and follows one overriding rule: optimize code for the
engineer who must understand and safely change it later.

## Readability

- Prefer direct code over clever code.
- Prefer named intermediate values over dense expressions.
- Keep functions focused and keep control flow shallow.
- Do not introduce a framework, template abstraction or design pattern without a
  concrete current need.
- Comments explain contracts and reasons. Code should explain the mechanics.

Formatting follows the repository .clang-format file. C and C++ use four spaces
per indentation level and never use tab characters for indentation. The
.editorconfig file gives editors the same rule. Types use PascalCase; functions
and variables use snake_case; private data members have a trailing underscore;
constants use a k prefix.

Run pixi run format to apply the canonical layout and pixi run format-check to
verify it without changing files. Formatting is mechanical and is not a matter
for individual interpretation during review.

## Ownership and lifetime

Ownership must be visible in the type and documented at public boundaries.

| Type | Contract |
|---|---|
| T | The receiver gets its own value. |
| const T & | Required non-owning borrow for the duration of the call. |
| T & | Required mutable non-owning borrow for the duration of the call. |
| T * | Nullable, non-owning access. Document the null case. |
| std::unique_ptr<T> | Exclusive ownership is transferred. |
| std::shared_ptr<T> | Ownership is genuinely shared or a ROS API requires it. |

Raw pointers never own data. Do not use explicit new or delete. A class that owns
a resource releases it through RAII in its destructor.

If an object retains a reference, pointer or view after a function returns, the
interface must state whose object it is and how long it must remain valid.

## Public contracts

Non-trivial public functions document the applicable parts of this template:

~~~cpp
/// One-sentence purpose.
///
/// Preconditions:
/// - Valid ranges, state and units required from the caller.
///
/// Ownership:
/// - What is borrowed, copied, moved or retained.
///
/// Returns:
/// - Meaning, units and validity of the result.
///
/// Errors:
/// - How every expected failure is reported.
///
/// Thread safety:
/// - Allowed callers and protected or thread-confined state.
~~~

Do not add empty contract sections to trivial accessors. Do specify units in
names or contracts, for example timeout_ms, speed_mps and angular_radps.

## Classes and abstractions

- Prefer values and small structs for data.
- Use a class when it enforces an invariant or owns a resource with a lifecycle.
- Use composition by default. Inheritance needs a concrete polymorphic reason;
  deriving a ROS component from rclcpp::Node is such a reason.
- Add an interface only for multiple real implementations or a necessary test or
  hardware boundary.
- Avoid singletons and global mutable state.
- Avoid a Pimpl unless binary compatibility or dependency isolation requires it.

## Errors

- Programmer errors and violated internal invariants are different from expected
  runtime failures.
- Expected network, decode and hardware failures must be returned explicitly and
  include enough context for diagnosis.
- Do not catch an exception only to ignore it.
- Do not invent a generic project-wide Result template before concrete call sites
  establish its requirements.

## Concurrency

Start with the single-threaded ROS executor. Before adding a thread, document:

- which object owns and joins it;
- which callbacks or functions run on it;
- which state is thread-confined;
- how shared state is synchronized;
- how shutdown and cancellation complete.

## Dependency boundaries

- ROS message types stay in the ROS-facing gateway layer.
- Zenoh types stay in the Zenoh transport layer.
- The wire protocol is a ROS- and Zenoh-independent C11 library.
- Mapping between layers uses small, explicit functions with unit tests.

Build the concrete path first. Extract abstractions from working code rather than
designing them speculatively.
