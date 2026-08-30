# Scripting

`Scripting` is Libft's deterministic custom scripting runtime. It provides a
small bounded expression language and typed native callbacks without exposing
raw host pointers or depending on Lua. The runtime is intentionally being
expanded in stages; existing Game and Voxel Lua bridges remain in migration
until parity tests are complete.

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
- `get_last_diagnostic()` reports the error code and source span of the last
  failed execution.
- `scripting_value_set_null()` and `scripting_value_set_integer()` construct
  callback results.

The current implementation is single-owner per engine instance. Immutable
future compiled modules may be shared, but mutable execution state must remain
isolated by caller-owned instances. Production code should include
`scripting.hpp` and must not depend on implementation details.
