# Lua

This module vendors the official Lua 5.4.8 runtime and builds it as a static
library for Windows, macOS, and Linux. The upstream source is kept unchanged
under `vendor/lua-5.4.8`. The MIT license in `LUA_LICENSE` applies only to those
vendored Lua sources; it does not change the license of Libft or Libft's own
integration code.

Lua is an internal runtime dependency. Libft-facing script registration,
sandbox policy, execution limits, and host callbacks are exposed by the Game
scripting bridge rather than directly through the upstream C API.
