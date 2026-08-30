# Scripting

`Scripting` is Libft's deterministic custom scripting runtime. It provides a
small bounded expression language and typed native callbacks without exposing
raw host pointers or depending on an external interpreter. Game and Voxel
production paths use this runtime; the line-command adapter remains only as a
temporary migration aid for existing assets.

## Current API

- `scripting_engine::initialize()` and `destroy()` manage the runtime lifecycle.
- `register_native()` registers a stable name and function-pointer callback.
- `set_operation_limit()` bounds native calls during one execution.
- `execute()` evaluates bounded expressions, typed literals, comparisons, and
  registered native calls.
- Comparison operators are `==`, `!=`, `<`, `<=`, `>`, and `>=`. Integer,
  boolean, string, and null equality is supported; ordering is defined for
  integers, booleans, and strings.
- `serialize_program()` and `deserialize_program()` use a canonical
  little-endian format with a magic value, version, exact lengths, a payload
  checksum, and no raw pointers. Loading is transactional and runs the
  bytecode verifier before replacing the caller's program.
- `if (condition) value else value` and `while (condition) value` are
  supported as expressions. Loop execution is bounded by the engine's
  operation limit; an unbounded condition cannot run indefinitely.
- Logical precedence follows conventional rules: unary `!`, then `&&`, then
  `||`.
- Locals declared with `let` can be reassigned with `name = expression`.
  Assignment leaves the resulting value on the expression stack and is
  therefore usable as a loop body.
- Braced blocks contain multiple expressions separated by semicolons. Their
  final expression is the block value; intermediate values are discarded and
  an empty block evaluates to `null`.
- `get_last_diagnostic()` reports the error code and source span of the last
  failed execution.
- `scripting_value_set_null()` and `scripting_value_set_integer()` construct
  callback results.

The current implementation is single-owner per engine instance. Immutable
future compiled modules may be shared, but mutable execution state must remain
isolated by caller-owned instances. Production code should include
`scripting.hpp` and must not depend on implementation details.

The Voxel terrain bridge executes terrain configuration scripts through this
custom runtime, including legacy line-oriented files after normalizing line
boundaries to statement separators. This keeps existing configuration assets
usable while removing Lua from terrain execution. The Game bridge defaults to
the custom runtime and temporarily retains its legacy line-command adapter
(`set`, `unset`, and `call`) for existing scripts. New production integrations
must use the custom runtime directly.
