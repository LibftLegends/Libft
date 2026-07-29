#Coding Guidelines

## Rule precedence

- Explicit exemptions override general rules.
- The authoritative contract sections (`Lifecycle`, `Error handling`, and `Thread-safety`) override older or more general wording.
- Per-module exceptions documented in their own section override the baseline class skeleton and other general guidance.

## Naming and formatting

Every return statement in a `void` function must be written as `return ;` (space before `;`), and every `void` function must end with an explicit `return ;`.
Every return statement in a non-`void` function must be written as `return (value);` (space before `(`).
Do not use for loops, ternary operators, or switch statements.
Indent code using 4 spaces per level.
After declaring a class, indent access specifiers (private, public, protected) by 4 spaces and indent member declarations within them by 8 spaces. This does not apply to structs.
In class declarations, place private members above public members and separate the sections with an empty line.
Function and variable names must use snake_case.
Constants and macros must use UPPER_SNAKE_CASE (all uppercase letters with `_` separators), for example `FT_ERR_SUCCESS`.
Do not use built-in width-ambiguous integer types such as `int`, `unsigned int`, `long`, `unsigned long`, `long long`, or `unsigned long long`.
Use project/fixed-width integer types instead (`ft_size_t`, `uint32_t`, `uint64_t`, `int32_t`, `int64_t`, and their signed/unsigned fixed-width variants).
Do not use built-in `bool`; use `ft_bool` (`typedef uint8_t ft_bool`) so boolean storage is always 8-bit.
Define boolean constants as `#define FT_FALSE ((ft_bool)0U)` and `#define FT_TRUE  ((ft_bool)1U)`.
Use full variable names instead of short ones or single letters, don't use s or str use string. Use names like file_descriptor and index; if multiple file descriptors or indexes are used, name each of them like file_descriptor_input, file_descriptor_output, index_height, index_width.

Use Allman style braces (opening brace on a new line).
Write `break` statements as `break ;` (a space between the keyword and the semicolon) to keep the spacing rules consistent.

## Class layout rules

In classes, member variable names must start with an underscore (_).
Within class member functions, access members and other methods using the this keyword.
Template classes may define member functions in the same header as the class declaration, but
other classes must split declarations into .hpp files and definitions into .cpp files. Inline and constexpr are allowed in headers for templates.
For template classes, the top section of the header must contain only the class declaration (member variables and method declarations).
Template member function bodies must not be defined inside the class declaration; they must be defined out-of-class in a separate section below the class declaration in the same header.
Do not define member function bodies inside the class declaration; place all definitions outside the class.
Every class must declare and define a default constructor and destructor. Do not use = default; explicitly define the bodies.
Lifecycle classes must explicitly delete copy and move constructors and must not define copy or move constructor bodies unless a module-specific exception is explicitly documented.
Specialized constructors are forbidden by default; keep only the default constructor unless a module-specific exception is explicitly documented.
When a specialized construction flow is required by exception, route fallible setup through `initialize(...)` instead of performing fallible work directly in that constructor body.
All classes must delete copy and move assignment operators by default. Only add assignment operators when a class explicitly needs assignment semantics and document that requirement. Operator overloading alone does not justify adding assignment operators.
Lifecycle classes must expose a dedicated explicit move helper named exactly `move` (for example `int32_t move(ft_string& other) noexcept`) and keep this name consistent across classes.
Functions must not return lifecycle class objects by value. When a function needs to create and return a lifecycle class instance, it must allocate the object on the heap, initialize it, and return an owning pointer or an explicitly documented ownership wrapper. If a raw pointer to a class is returned, any error path must destroy and free partial state and return a null pointer (`ft_nullptr` / `nullptr`).

Assignment operator exceptions:
- `ft_string`: copy/move assignment is allowed for required string value semantics used by stream/conversion APIs.
- `ft_big_number`: copy/move assignment is allowed for required arithmetic/proxy value semantics across the module.
- `ft_map`, `ft_unordered_map`, `ft_sharedptr`, and `ft_uniqueptr`: copy/move assignment may be kept when needed for compatibility and required value/ownership semantics in existing call chains.
- `ft_function`: copy/move assignment is allowed for callable wrapper value semantics and compatibility in callback pipelines.
- `Pair` (`Template/pair.hpp`): copy/move assignment is allowed for pair value semantics in existing map/test call chains.
- `Pool<T>::Object` (`Template/pool.hpp`): move assignment is allowed for pool handle transfer semantics.
- Template proxy wrappers (for example `mapped_proxy`) may keep compatibility assignment operators used for operator-chain error propagation.

Operator-overload and proxy exception:
- Keep useful access/operator-chain overloads for `ft_vector`, `ft_map`, `ft_unordered_map`, `ft_sharedptr`, `ft_uniqueptr`, `ft_string`, `ft_big_number`, the SCMA handle class, and documented Math classes that already expose operator-based value semantics, including `operator[]`, `operator->`, and related dereference/value access overloads.
- For classes that expose these operator-chain access paths, keep proxy classes (or equivalent proxy return wrappers) when needed to preserve error propagation across chained calls.
- These overloads/proxy wrappers are explicitly allowed and are not considered violations of the default "delete assignment operators by default" rule when they serve error propagation or API compatibility.

### Class Skeleton Baseline

Use this baseline skeleton for classes (extend only when needed):

```cpp
typedef uint8_t ft_bool;
#define FT_FALSE ((ft_bool)0U)
#define FT_TRUE  ((ft_bool)1U)

class ft_example
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        static thread_local int32_t _last_error;
        uint8_t _initialised_state;
        pt_recursive_mutex *_mutex;
        static int32_t set_error(int32_t error_code) noexcept;

    public:
        ft_example() noexcept;
        ft_example(const ft_example &other) noexcept = delete;
        ft_example(ft_example &&other) noexcept = delete;
        ~ft_example() noexcept;

        ft_example &operator=(const ft_example &other) noexcept = delete;
        ft_example &operator=(ft_example &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_example &other) noexcept;
        int32_t initialize(ft_example &&other) noexcept;
        int32_t destroy() noexcept;
        int32_t move(ft_example &other) noexcept;

        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};
```

Notes:
- This skeleton is the default for lifecycle-style classes. Keep existing documented exemptions (proxy classes, `ft_nullptr` stand-ins, `Template/pair.hpp`, and `Template/function.hpp`).
- Use `pt_recursive_mutex *` for class-owned mutexes, never raw `pthread_mutex_t *`.
- `pt_recursive_mutex` and `pt_mutex` serve different mutex types. For class-owned mutex fields, use `pt_recursive_mutex *` and the recursive lock/unlock helpers.
- Include `_last_error` helpers only for classes that expose `_last_error` behavior.

## Lifecycle contract (Authoritative)

These rules are authoritative for lifecycle classes and override older conflicting wording.

Lifecycle scope:
- A class is a lifecycle class if any of the following are true:
  - it owns heap memory or other external resources
  - it owns a mutex (including heap-allocated class mutexes)
  - initialization can fail
  - destruction must release resources
  - it exposes `initialize(...)` / `destroy()`
  - it can enter a meaningful failed state independent of caller misuse
- Lifecycle enforcement is mandatory for all lifecycle classes except explicitly exempt categories listed below.
- Explicit exemptions:
  - Proxy classes are exempt from lifecycle and thread-safety ownership requirements; proxies must be designed so proxy operations themselves do not fail unexpectedly, while underlying class operations may fail and report errors explicitly.
    - Proxy classes are not required to own a mutex, expose lifecycle helpers (`initialize(...)`, `destroy()`, `move(...)`), or follow lifecycle state-field shape requirements.
    - Proxy classes are allowed to remain lightweight failure-propagation carriers for operator chains; their purpose is to forward/report underlying errors without introducing additional meaningful failure modes.
    - Prefer minimal, low-dependency proxy implementations that avoid extra ownership layers and avoid adding synchronization/lifecycle machinery unless a proxy-specific contract explicitly requires it.
    - A class is considered a proxy class only when its class name explicitly contains `proxy`.
- `ft_nullptr`/`nullptr` stand-in utility classes are exempt from lifecycle enforcement and optional thread-safety helper requirements because they have no meaningful failure state and must complete their local task without failure.
- Non-owning polymorphic behavior-node base classes that only carry thread-local diagnostic state may be explicitly documented as non-lifecycle classes; they are immediately usable after construction and their diagnostic accessors do not require lifecycle-state guards.
  - `ft_nullptr` must explicitly behave as a null-pointer literal stand-in: assigning it to any pointer type must set that pointer value to `0` (null), equivalent to `nullptr`.

Lifecycle state:
- Classes with lifecycle management must keep an explicit 8-bit state field named exactly `_initialised_state` with type `uint8_t`:
  - `0`: uninitialised
  - `1`: destroyed
  - `2`: initialised
- Do not duplicate per-class lifecycle state constants. Define and reuse shared generic lifecycle state constants in Errno/internal helpers (for example `FT_CLASS_STATE_UNINITIALISED`, `FT_CLASS_STATE_DESTROYED`, `FT_CLASS_STATE_INITIALISED`) so classes do not repeat boilerplate numeric definitions.
- Default constructors must never fail or abort.
- Default constructors may only perform non-fallible setup that leaves the object in an uninitialised-ready state (`state = 0`).
- Constructors may zero/reset fields into a safe known uninitialised-ready state (`state = 0`), but must not perform fallible setup.
- Member-class objects must not be lifecycle-initialised in the constructor; they remain uninitialised until the owning class `initialize(...)` runs and initializes them explicitly.
- Assignment operators and value setters (for example `operator=`, `set_*`) must not implicitly initialize an uninitialised lifecycle object; callers must call `initialize(...)` first, and assignment/value operations may assume the object is already initialised.
- All fallible setup belongs in `initialize(...)`.
- Lifecycle class instances may be created on stack or heap; in both cases, callers are responsible for calling `initialize(...)` before first use.

## Error handling contract (Authoritative)

Lifecycle error signaling:
- Add a dedicated not-initialised error code in the Errno module (for example `FT_ERR_NOT_INITIALISED`) and use it consistently.
- Classes that expose `_last_error` must follow the single authoritative `#_last_error Contract` section below (including required prototype `set_error(int32_t error_code)`).
- An uninitialised object is not a valid access point for class error-state queries, even though `_last_error` exists independently as thread-local storage.
- `get_error()` and `get_error_str()` must abort via `su_abort();` when `_initialised_state == FT_CLASS_STATE_UNINITIALISED`.
- Calling `get_error()` and `get_error_str()` in destroyed state (`_initialised_state == FT_CLASS_STATE_DESTROYED`) is valid and must report based on `_last_error`.
- Use a separate lifecycle-state helper (for example `is_initialised()`) when callers need to validate whether an object is initialised.
- `get_error()` and `get_error_str()` must be instance methods (non-static). Static class-scope error queries (for example `ft_string::get_error()`) are forbidden; callers must query a concrete object instance (for example `value.get_error()`).

Shared lifecycle abort helper:
- Lifecycle abort formatting/behavior must be centralized in shared Errno internal helpers instead of per-class duplicated implementations.
- These helpers are for internal use only and must be declared in `Errno/errno_internal.hpp`.
- Required internal helper prototypes:
  - `void errno_abort_if_uninitialised_or_destroyed(uint8_t initialised_state, const char *method_name);`
  - `void errno_abort_lifecycle(uint8_t initialised_state, const char *method_name, const char *reason);`
- Usage rules:
  - `errno_abort_if_uninitialised_or_destroyed(...)` is used for the common guard path where a method requires `state == 2` and no custom reason text is needed.
  - `errno_abort_lifecycle(...)` is used when a specific reason must be attached (for example invalid source state in copy/move flows).
  - Classes must pass lifecycle state and method context into these helpers; classes must not format lifecycle abort messages locally.
- The helper prints a clear message to `stderr` and calls `su_abort();`.
- `su_abort();` is non-returning by contract: once called, execution must never continue in the caller path.

Initialize / destroy / destructor rules:
- `initialize()` aborts only when called while already initialised (`state == 2`) unless a class explicitly documents a different contract.
- Copy/move style initialization (`initialize(const T&)`, `initialize(T&&)`, explicit `move(...)`, and any explicitly-enabled copy/move assignment) must follow:
  1. Source must be initialised (`state == 2`) or lifecycle abort helper is invoked.
  2. `this == &other` is a no-op success.
  3. Destination may be uninitialised, destroyed, or initialised.
  4. If destination is initialised, call `destroy()` first and honor its return value.
- If initialization fails after partial work, the object must end in destroyed state (`state = 1`).
- `destroy()` must return error codes (no abort).
- If `destroy()` is called while the class is uninitialised (or already destroyed), it is a no-op and must return `FT_ERR_SUCCESS`.
- If the object is initialised and thread safety is enabled, `destroy()` must call `disable_thread_safety()` as the first cleanup step.
- `destroy()` must always continue cleanup of all owned resources even after a failure; if multiple failures occur, return only the first error code encountered.
- Destructors must fail silently (no abort), and are valid in uninitialised/destroyed states.
- For initialised objects, destructors must call `destroy()`; `destroy()` is responsible for running `disable_thread_safety()` first.
- Destructors must always run cleanup to completion: if intermediate steps fail, continue cleanup as best-effort and do not stop early.
- Destructor cleanup must leave class fields reset to an uninitialised-ready state (so `initialize(...)` could be called again in theory), even though destructor-time reinitialization should not be relied on.

Copy/move safety rules:
- For copy/move paths, perform all potentially failing work first.
- Only after success, apply non-failing state swaps/commits.
- Source remains unchanged until the failing phase is complete.
- On any copy/move failure (allocation failure, mutex/setup failure, or any reported sub-call failure), destination must be left in destroyed state (`state = 1`).
- Lifecycle classes must not use constructor-based copy/move. Use explicit `initialize(const T&)`, `initialize(T&&)`, or `move(...)` flows so failures can be reported by return code instead of hidden constructor state.
- Source state handling:
  - Source in uninitialised state (`state = FT_CLASS_STATE_UNINITIALISED`) is lifecycle misuse and must trigger lifecycle abort behavior.
  - Source in destroyed state (`state = FT_CLASS_STATE_DESTROYED`) is allowed for copy/move propagation paths; destination must end in destroyed state as well.
  - Source `_last_error` state is propagated as part of copy/move behavior when the class exposes `_last_error`.

Lifecycle return ownership:
- Lifecycle classes may still be declared as local variables inside functions while constructing or testing state, but lifecycle objects must not cross function boundaries by value.
- Public and internal functions that previously returned a lifecycle class by value must be migrated to return a heap-allocated instance (`T *`) or a project ownership wrapper whose ownership contract is explicit in the API.
- Heap-returning lifecycle factory functions must leave no initialized heap object behind on failure. On any error or initialization failure, call `destroy()` when needed, release the allocation, and return a null pointer (`ft_nullptr` / `nullptr`).

Sub-object initialization:
- When a class initializes multiple sub-objects, it must call `initialize(...)` on each required sub-object.
- If one sub-object initialization fails, destroy all already-initialised sub-objects before returning.
- Parent object ends in destroyed state (`state = 1`) after failed initialization.

## Thread-safety contract (Authoritative)

Thread-safety ownership and mutex requirements:
- Thread-safety helpers are required for classes that own internal state that can change after initialization and may be read or modified concurrently.
- If one thread can modify internal members while another thread can read or modify those same members at the same time, the class must provide mutex-based synchronization unless the class is explicitly documented as externally synchronized.
- Thread-safety helpers are not required for classes that become immutable after initialization.
- Thread-safety helpers are not required for classes used only in a single-threaded design.
- Thread-safety helpers are not required for classes explicitly documented as requiring caller-side synchronization.
- Thread-safety helpers are not required for exempt categories such as proxy classes and documented utility exceptions.
- `nullptr` utility/wrapper classes are an explicit exempt example because they do not hold mutable shared state.
- Optional thread-safe classes expose `enable_thread_safety()`, `disable_thread_safety()`, and `is_thread_safe()`.
- `_mutex` remains private and starts as `ft_nullptr`; `is_thread_safe()` is authoritative (`_mutex != ft_nullptr`).
- The mutex must be created/initialised only when the user explicitly calls `enable_thread_safety()`.
- Mutex allocation/ownership rule:
  - Class mutexes must always be recursive mutexes.
  - The mutex must always be heap-allocated.
  - For regular classes, the mutex is instance-local and must not be shared across instances.
  - Exception: the SCMA module may use one shared global mutex, and that global mutex must also live on the heap.

Enable/disable behavior:
- `enable_thread_safety()` must be safely retryable. If enable fails, object state remains valid for later retries.
- `disable_thread_safety()` destroys and deallocates the mutex and must set `_mutex = ft_nullptr` before returning, even when teardown reports an error.
- Mutex teardown must happen through `disable_thread_safety()`, including when called from `destroy()` or from destructor cleanup paths.
- If teardown fails, return the error code but still leave the class in a retry-safe state.

Locking rules:
- Use shared pthread safe lock/unlock helpers from `PThread/pthread_internal.hpp` for nullable mutex pointers; no RAII lock guards.
- The helper families cover different pointer types and both safely handle nullable pointers (`ft_nullptr` / `nullptr`) to reduce repetitive lock/unlock boilerplate across the library.
- Use `pt_recursive_mutex_*_if_not_null(...)` for recursive mutex pointers (including class-owned mutex fields).
- Use `pt_mutex_*_if_not_null(...)` for regular non-recursive mutex pointers (for non-class and class code where a regular mutex type is intentionally used).
- Internal helper prototypes to use:
  - `uint32_t pt_mutex_lock_if_not_null(const pt_mutex *mutex_pointer)`
  - `uint32_t pt_mutex_unlock_if_not_null(const pt_mutex *mutex_pointer)`
  - `uint32_t pt_recursive_mutex_lock_if_not_null(const pt_recursive_mutex *mutex_pointer)`
  - `uint32_t pt_recursive_mutex_unlock_if_not_null(const pt_recursive_mutex *mutex_pointer)`
- For multi-object operations, always lock mutexes in ascending memory-address order (lowest address first) and unlock explicitly in reverse order.
- Lock acquisition failures are operation failures and must be converted to `FT_ERR_*` return codes by the caller.
- Unlock return values must not be checked by callers and must always be ignored explicitly with `(void)pt_*_unlock_if_not_null(...)`.
- Assume unlock succeeds; do not add unlock-error handling branches or propagate unlock return codes.

Dedicated testing-only mutex exposure helpers are not required. Prefer the `Testing-only rules` section below.

## Explicit exemptions

- Proxy classes are exempt only as documented in `Lifecycle scope`.
- `ft_nullptr`/`nullptr` stand-ins are exempt only as documented in `Lifecycle scope`.
- `pt_mutex` and `pt_recursive_mutex` are exempt from the class-owned mutex shape rule that requires class mutex fields to be `pt_recursive_mutex *`, because they are the core mutex primitives and must remain thread-safe by default without extra optional thread-safety layers.

### PThread low-level compatibility exemptions

- The low-level `PThread` module is a compatibility/primitive layer and is exempt from the fixed-width replacement rule for C/POSIX-facing ABI types. In this module, using `int`, `bool`, `size_t`, `long`, `unsigned long long`, and related native types is allowed when required for ABI compatibility or existing public API stability.
- In the `PThread` module, C-style return conventions (`0`/non-zero or `-1`) are allowed for compatibility wrappers and legacy synchronization/task-scheduler helpers; the strict `FT_ERR_*`-only return contract applies to new higher-level modules unless a `PThread` API is explicitly migrated.
- Legacy public symbol names in `PThread` (including existing non-snake-case method names) are exempt from the snake_case naming rule when renaming would break compatibility.
- `pt_condition_variable`, `ft_thread`, `ft_task_scheduler`, and related `PThread` internals are exempt from the class-owned mutex shape rule requiring `pt_recursive_mutex *` fields.
- `PThread/lock_guard.hpp`, `PThread/unique_lock.hpp`, and `PThread/pthread_errno_guard.hpp` are compatibility utilities and are exempt from the "no RAII lock guards" and deprecated error-stack removal requirements while they remain part of supported API surface.
- Unlock result handling rules in `PThread` low-level primitives may follow existing implementation semantics where platform/system unlock failures are explicitly surfaced; the strict "always ignore unlock return values" rule applies to higher-level module call sites using `pt_*_unlock_if_not_null(...)`.
- Lifecycle class-skeleton baseline requirements are relaxed for existing `PThread` compatibility classes: legacy assignment operators, legacy move helper names (for example `initialize_move(...)`), and existing constructor sets are allowed where required to preserve ABI/API behavior.
- Existing `PThread` lifecycle state constants defined inside classes are allowed and do not need to be migrated to shared Errno lifecycle constants unless that class is being actively refactored.

### Compatebility low-level platform exemptions

- In the `Compatebility` module, platform-mandated callback/signature types may use native ABI forms where required by OS APIs (for example signal-handler signatures using `int`).
- In the `Compatebility` module, the following low-level service/syslog compatibility wrappers may keep documented C/POSIX-style status returns (`0`, `1`, `-1`) for compatibility with existing call chains: `cmp_service_set_working_directory`, `cmp_service_detach_process`, `cmp_service_release_console`, `cmp_service_install_signal_handlers`, and `cmp_syslog_open`.
- `cmp_secure_memzero` is exempt from the strict `FT_ERR_*` return contract and may keep `0`/`-1` compatibility status semantics.
- Platform sound backend classes (`ft_sound_device_alsa`, `ft_sound_device_coreaudio`, `ft_sound_device_win32`) are exempt from lifecycle class-skeleton enforcement (`_initialised_state`, `initialize(...)`, `destroy()`, `move(...)`, and class-owned mutex helper shape) while they remain thin backend adapters behind the `DUMB` sound-device abstraction.

### System_utils compatibility exemptions

- In the `System_utils` module, existing low-level compatibility wrappers may keep documented C/POSIX-style status returns (`0` success, `-1` failure) for public `su_*`/`ft_*` APIs where compatibility with existing call chains and syscall-like behavior is required.
- In the `System_utils` module, existing process-wide registry internals in `System_utils_health.cpp` and `System_utils_resource_tracer.cpp` may keep direct `std::mutex` lock/unlock usage while those registries remain stdlib container-backed compatibility implementations.

### Math constructor compatibility exemptions

- In the `Math` module, value-oriented linear-algebra/autodiff classes may keep existing specialized value constructors for API ergonomics and compatibility (`vector2`, `vector3`, `vector4`, `matrix2`, `matrix3`, `matrix4`, `quaternion`, `ft_dual_number`).
- These specialized constructors must keep constructor bodies thin and route fallible setup through `initialize(...)`.
- `ft_dual_number_proxy` may keep compatibility specialized constructors used for explicit error/value forwarding in operator-chain proxy flows.

### Printf floating-point compatibility exemptions

- In the `Printf` module, `long double` usage is allowed where required for `%Lf` and related `printf`-compatible formatting/parsing behavior.

### Template function simplification

`Template/function.hpp` defines `ft_function`. Keep this template as a lightweight callable wrapper
with no recursive mutex, errno helpers, or thread-safety toggles. Any new behavior should leave
error handling and synchronization to callers rather than reintroducing state inside `ft_function`.

### Template constructor compatibility exemptions

In the `Template` module, existing value/container APIs may keep compatibility specialized constructors where those constructors are already part of established public usage. This exemption applies to:
- `ft_vector`, `ft_deque`, `ft_set`, `ft_map`, `ft_unordered_map`, `ft_graph`, `ft_matrix`, `ft_circular_buffer`, and `ft_event_emitter` capacity/value configuration constructors.
- `ft_optional`, `ft_variant`, and `ft_tuple` value-forwarding constructors used by template value semantics.
- `ft_string_view` pointer/size view constructors.
- `ft_sharedptr`, `ft_uniqueptr`, `ft_future`, and `Pool<T>::Object` compatibility constructors required by ownership/adapter call chains.
- `ft_function` callable constructor and `Pair` key/value constructors.
- Proxy/helper constructors in Template proxy classes (`reference_proxy`, `const_reference_proxy`, `value_proxy`, `char_proxy`, and similar proxy wrappers).

These specialized constructors must keep constructor bodies thin and continue routing fallible setup through `initialize(...)` where applicable.

### Template naming compatibility exemptions

In the `Template` module, legacy public type names `Pair` (`Template/pair.hpp`) and `Iterator` (`Template/iterator.hpp`) are exempt from the snake_case naming rule for compatibility with existing API/test call chains.

### Deprecated Errno Stack

`ft_errno` and the legacy thread-local global error stack helpers (`ft_global_error_stack_*`,
`ft_error_stack`, `ft_operation_error_stack`, etc.) are deprecated. Do not add them back to any
new code. Remove references if you encounter them, and rely on return codes or other explicit error
reporting mechanisms instead.
Anything related to error-stack concepts is deprecated as well (including helper wrappers,
aliases, compatibility shims, and stack-style push/pop accessors) and should be removed when
encountered.

### Deprecated Last-Operation Error API

Legacy helpers named `last_operation_error`, `last_operation_error_str`, and
`set_last_operation_error` are deprecated and must be removed from code when touched.
Use the `_last_error Contract` naming only: `set_error(int32_t error_code)`, `get_error()`,
and `get_error_str()`.

### Error Code Definition Contract

All functions that return an error code must return one of the following only:
- `FT_ERR_SUCCESS`
- `FT_ERR_*`

Functions must never invent ad-hoc numeric error codes outside the Errno module.
Any new error code must be defined in the Errno `.hpp` file before use.

### Signed Error Severity Model (Authoritative)

Use signed error semantics consistently across the project:
- `error_code < 0`: critical failure (for example memory allocation failures, lock/mutex failures, or low-level/internal/system failures).
- `error_code == 0`: success (`FT_ERR_SUCCESS`).
- `error_code > 0`: recoverable/non-critical outcome (for example not found, already exists, out of range, empty/full, retryable/would-block style outcomes).

Checking rules:
- First-level failure check remains `error_code != 0`.
- When behavior differs by severity, classify after the non-zero check:
  - critical path: `error_code < 0`
  - recoverable path: `error_code > 0`
- Do not treat all non-zero codes as critical by default.

### Initialize Return Contract

`initialize(...)` methods must return `int32_t` and follow the Error Code Definition Contract.

## File/module naming

Only .cpp files must be prefixed with the name of the module they belong to. For nested modules, filenames must always use the parent module name followed by the nested module name.
For .hpp files, prefix only those meant for internal use with the module name.
Generic headers are headers used widely across the module that primarily collect many simple function prototypes and shared module-level declarations (module-wide API/utility/umbrella headers). These may use the module name (for example `math.hpp`, `networking.hpp`, `api.hpp`), while class headers should use the class name as the filename (for example `class_string.hpp`, `socket_handle.hpp`).
Concrete examples:
- `.cpp` module-prefixed filenames: `Errno/errno_set_error.cpp`, `PThread/pthread_mutex_lock.cpp`, `Compatibility/compatibility_get_time.cpp`
- Class `.cpp` filenames: `CPP_class/cpp_class_file_stream.cpp`, `Game/game_server.cpp`
- `.cpp` nested-module filenames: `PThread/Recursive/pthread_recursive_mutex_init.cpp`, `Math/Vector/math_vector_normalize.cpp`, `Network/Tcp/network_tcp_connect.cpp`
- Internal `.hpp` module-prefixed filenames: `Errno/errno_internal.hpp`, `PThread/pthread_internal.hpp`, `CMA/cma_internal.hpp`
- Generic module-wide `.hpp` filenames: `Errno/errno.hpp`, `Math/math.hpp`, `Networking/api.hpp`
- Class header filenames: `String/string.hpp`, `Socket/socket_handle.hpp`, `Containers/class_vector.hpp`

Update documentation when public functionality is added or changed. When a module gets new public functionality, update that module's `README.md` with the new functions, classes, structs, methods, and a short explanation. Bug fixes alone do not require README updates unless they introduce unavoidable behavior or contract changes that users must know about.
When a module is added or a module change adds, removes, or reroutes inter-module dependencies, update `Docs/module_dependency_graph.md` in the same change set so the tracked dependency graph stays current.
When adding a new module, add that module to the root `README.md` and create the module's own `README.md` documenting its public-facing functions, classes, structs, methods, and purpose.

Code that relies on platform-specific features must place only the platform-dependent portions into helper functions in the Compatibility module. Platform-specific means the code cannot run on all of Linux, Windows, and macOS.

## Test file organization

Prefer multiple focused test files over one large mixed-purpose test file.
Each test file name must make the tested class, module, or function family clear
from the filename. For example, tests for `chunk_mesh` belong in a
`test_chunk_mesh.cpp` file, while tests for `terrain_generate_chunk` belong in a
`test_terrain_generator.cpp` file. Keep unrelated class/function tests split so
the purpose of each file is obvious from its naming convention.

## Testing-only rules

- For testing builds only, all class private members must be exposed as `public` under `#ifdef LIBFT_TEST_BUILD` so tests can validate expected internal state and behavior directly. This must never be exposed in production builds.
- Dedicated testing-only mutex exposure helpers are not required. Prefer test-build member exposure through `LIBFT_TEST_BUILD`; existing per-class mutex testing helpers should be removed unless a class explicitly documents a temporary exception.
- Test filenames must use snake_case and start with the module name.
- Function-focused test files must follow: `test_<module_name>_<function_name>.cpp`.
- Class-focused test files must follow: `test_<module_name>_<class_name>_<method_name>.cpp`.
- Each class method must have its own dedicated test file when the file focuses on that single method.
- If one file intentionally covers multiple methods for a class, use a general goal suffix: `test_<module_name>_<class_name>_<general_goal>.cpp`.
- Lifecycle-focused class tests may use: `test_<module_name>_<class_name>_lifecycle.cpp`.
- Header helpers for tests (`.hpp`) must follow the same naming structure.

### Allocation failure test pattern

When using `cma_set_alloc_limit` in tests to force allocation failures:
- Use a non-zero limit to trigger failures (`0` means no allocation limit).
- Set the limit once at the start of the test (or test section), then run the failure scenario.
- Reset it exactly once to `0` at the end of the test (or section) to avoid leaking state across tests.
- Do not repeatedly set the same limit before and after a single call unless the test is explicitly stepping through multiple failure depths.

## Build and test timing

Building the library and running the full test suite typically takes about two and a half minutes, possibly a little longer.
When running the `libft_tests` test runner, always hide successful tests to avoid noisy output:
`FT_TEST_HIDE_SUCCESSFUL=1 ./libft_tests` (or `FT_TEST_HIDE_SUCCESSFUL=1 ./Test/libft_tests` from the repository root).
Successful test lines are unnecessary; inspect failures and the final summary only.

### Locking guidelines

This section defers to the authoritative `Locking rules` above.

### CMA Error Handling Rules

Within the CMA module, only the documented public-facing entry points should surface error signals to their callers; helper utilities should propagate failures through the module’s internal mechanisms.

### SCMA Error Handling Rules

Within the SCMA module, only the listed public-facing helpers should expose errors directly; all other utilities rely on the module’s internal reporting to keep overall state coherent.
SCMA classes do not need a per-class local mutex when the module-level global mutex already provides the required synchronization; prefer the SCMA global mutex in that case.
- `_last_error` access and behavior are defined only in the authoritative `#_last_error Contract` section below.

### `_last_error` Contract

When a class maintains a `_last_error` field, treat it as thread-local diagnostic state rather than a shared global-style error channel.
- `_last_error` must be thread-local per thread of execution. Code must not rely on a single shared `_last_error` value across threads.
- Declare `_last_error` as `static thread_local int32_t _last_error` so each thread has its own copy while the class exposes a single helper suite.
- `_last_error` must be initialised to `0` (`FT_ERR_SUCCESS`) once at thread/program startup for that thread-local instance, not per object instance.
- Constructors may set `_last_error` to `FT_ERR_SUCCESS` while establishing a new object in the uninitialised state. They must not overwrite an error state being propagated from an already-initialised source or otherwise replace an existing initialised object's error state.
  - If constructor/destructor internals invoke helper paths that may update `_last_error`, preserve and restore the previously observed value whenever the object is already initialised or the value is being propagated.
- Destructors must never modify `_last_error`.
- Classes that expose `_last_error` behavior must provide `int32_t set_error(int32_t error_code)`, `get_error()`, and `get_error_str()` helpers. `set_error(int32_t error_code)` is the only writer for `_last_error`.
- `set_error(int32_t error_code)` must be private in production builds; test builds may access it only through `LIBFT_TEST_BUILD` private-section exposure.
- `get_error()` and `get_error_str()` are strictly read-only and must never modify `_last_error`.
- Direct assignment to `_last_error` is forbidden outside `set_error(...)`; internal code must not write `_last_error` directly.
- `set_error(int32_t error_code)` must store exactly the error code passed by the caller and return that same code.
- `get_error()` / `get_error_str()` are the sanctioned public interfaces for reading class error state across classes when required. Other classes must not read `_last_error` directly.
- `get_error()` / `get_error_str()` must abort via `su_abort();` when `_initialised_state == FT_CLASS_STATE_UNINITIALISED`.
- `get_error()` / `get_error_str()` are valid when `_initialised_state == FT_CLASS_STATE_DESTROYED` and must then report `_last_error` normally.
- If a called method does not return an error code (for example `void` methods or value-returning methods without status), callers must read `get_error()` to obtain the operation error result.
- New code should still return error codes directly from methods that can fail. `_last_error` is a secondary diagnostic/compatibility channel, not the primary error transport.
- `_last_error` may be stale relative to earlier operations; lifecycle state is the authoritative indicator of constructor copy/move success.
- Any method callable by the user must set `_last_error` on every exit path (`FT_ERR_SUCCESS` on success, exact error code on failure), except `get_error()`, `get_error_str()`, the default constructor, the destructor, copy constructor paths, and move constructor paths. Private helpers should generally return errors upward and let the public entry point set `_last_error`.
- This requirement includes const query-only methods and methods that are not expected to fail; on successful exit they must set `_last_error` to `FT_ERR_SUCCESS`.
- `is_thread_safe()` must set `_last_error` on every exit path as well (`FT_ERR_SUCCESS` on success).
