# Template

The `Template` module provides the library's generic containers, ownership wrappers, callable/promise/future types, algorithms, type utilities, and serialization helpers.

## Algorithms and Utilities

- `algorithm.hpp` - Generic helpers such as find/count/sort-style algorithms used by containers and tests.
- `constructor.hpp` - `construct_at`, `construct_default_at`, and destruction helpers used by containers.
- `move.hpp`, `swap.hpp`, and `static_cast.hpp` - Project wrappers for move, swap, and cast operations.
- `template_concepts.hpp`, `invoke.hpp`, and type-trait helpers in `pair.hpp`, `map.hpp`, `vector.hpp`, and `variant.hpp` - Compile-time checks used to select initialization, destruction, invocation, and proxy paths.
- `container_serialization.hpp` - Helpers for serializing supported containers.
- `math.hpp` - Template numeric helpers.

## Core Value Types

- `Pair<Key, Value>` - Two-value container with public first/second-style storage, constructors, lifecycle-aware initialization helpers, and assignment for map/test compatibility.
- `ft_status` - Error/status value used by `ft_result`.
- `ft_result<ValueType>` - Result wrapper containing either a value or error status.
- `ft_optional<ValueType>` - Optional value with lifecycle, value construction/reset, presence checks, access proxies, and error propagation.
- `ft_variant<Types...>` - Tagged union with reset, type-selecting `get`, value proxies, thread-safety helpers, and error accessors.
- `ft_tuple<Types...>` - Tuple value wrapper for heterogenous values.
- `ft_string_view` - Non-owning string view with size/data access and character proxy support.

## Sequence Containers

- `ft_vector<ElementType>` - Dynamic array with lifecycle, copy/move initialization, reserve/resize, push/pop, index operators, insert/erase, iterators, size/capacity/empty, error accessors, and optional thread safety. `ft_vector_inline_storage` provides small-buffer storage when the element type is complete. `replace_from(other)` replaces the live elements of an initialized vector and keeps the previous elements if copying fails. A replacement may allocate temporary storage. The overload that accepts a staged vector and a relocation callback transfers heap storage or relocates inline elements, empties the source, and preserves both vectors' thread-safety settings.
- `ft_deque<ElementType>` - Double-ended queue with push/pop front/back, front/back access, size/empty/clear, lifecycle, error accessors, and optional thread safety. `deque_node` is its linked node.
- `ft_circular_buffer<ElementType>` - Fixed-capacity ring buffer with push/pop, full/empty checks, size/capacity, clear, lifecycle, error accessors, and optional thread safety.
- `ft_queue<ElementType>` - FIFO queue with push/pop/front/back style access, value proxies, lifecycle, error accessors, and optional thread safety. `QueueNode` is its linked node.
- `ft_stack<ElementType>` - LIFO stack with push/pop/top, size/empty/clear, lifecycle, error accessors, and optional thread safety. `StackNode` is its linked node.
- `ft_priority_queue<ElementType>` - Priority queue with push/pop/top, empty/size, lifecycle, and error accessors.

## Associative Containers

- `ft_map<Key, MappedType>` - Ordered map with insert/erase/find, `operator[]`, mapped proxy support, size/empty/clear, iterators where provided, lifecycle, error accessors, and optional thread safety. The const and mutable `visit_value(key, callback)` overloads call a nonthrowing callback under the map lock and return its status, or `FT_ERR_NOT_FOUND` when the key is absent. The callback may access the mapped value but must leave the map structure intact. Use `erase_at(index, relocate)` to erase a known insertion-order position and shift its suffix. The relocation callback must overwrite the destination without failing and leave the source destructible. Synchronize an index obtained separately with its erasure. An index outside the map returns `FT_ERR_OUT_OF_RANGE`. Use `insert_unique(entry, relocate)` to insert a prepared key-value pair. A duplicate key returns `FT_ERR_ALREADY_EXISTS`. On failure the map and prepared pair stay intact. The callback transfers each entry into a default-constructed destination and leaves the source destructible.
  `remove_transactional(key)` preserves the original map if allocating storage or copying a retained entry fails. Successful non-tail removal may invalidate all borrowed pointers. An optional nonthrowing transfer callback runs after staging to retain state that copying omits. An optional nonthrowing predicate can certify that both destroying and copying each shifted value cannot fail, allowing removal in place.
- `ft_unordered_map<Key, MappedType>` - Hash map with buckets, iterators, insert/erase/find, `operator[]`, mapped proxy support, size/bucket count, lifecycle, error accessors, and optional thread safety. Public helper types include `ft_pair`, `iterator`, `const_iterator`, and `mapped_proxy`.
  `erase_and_copy` copies a mapped value into independent output and erases the entry under one lock and lookup. It requires nonthrowing copy assignment and leaves the output intact if the key is missing.
- `ft_set<ValueType>` - Set container with insert/erase/find/contains style operations, size/empty/clear, lifecycle, and optional thread safety.
- `ft_trie<ValueType>` - Prefix tree with insert/find/remove/prefix traversal style operations. `node_value` stores trie node payload state.

## Graphs, Matrices, Pools, and Events

- `ft_graph<VertexType>` - Graph container with vertex/edge insertion, BFS, DFS, neighbor queries, size/empty/clear, lifecycle, error accessors, and optional thread safety. `graph_node` stores vertex and edge data.
- `ft_matrix<ValueType>` - Generic matrix with dimensions, element access, resize/clear, lifecycle, error accessors, and optional thread safety.
- `Pool<T>` - Object pool with acquire/release and lifecycle controls.
- `Pool<T>::Object` - Move-only pool handle that returns objects to the pool.
- `ft_event_emitter<EventType, Args...>` - Event dispatcher with listener registration, emit, listener removal, size/empty/clear, lifecycle, error accessors, and optional thread safety. `Listener` stores event/callback pairs.

## Ownership and Callable Types

- `ft_sharedptr<T>` - Reference-counted pointer with copy/move/value semantics, dereference/arrow/reference proxies, reset/release-like operations, error accessors, and optional thread safety.
- `ft_uniqueptr<T>` - Unique ownership pointer with move semantics, dereference/arrow proxies, release/reset behavior, error accessors, and optional thread safety.
- `ft_function<Signature>` - Lightweight callable wrapper with copy/move construction and assignment, call operator, and boolean conversion.
- `ft_promise<ValueType>` / `ft_promise<void>` - Promise state for asynchronous completion, value/error setting, future retrieval, lifecycle, error accessors, and optional thread safety.
- `ft_future<ValueType>` / `ft_future<void>` - Future endpoint with `get`, `wait`, `valid`, lifecycle, error accessors, lock helpers, and optional thread safety.

## Proxy and Trait Types

Proxy classes such as `reference_proxy`, `const_reference_proxy`, `value_proxy`, `char_proxy`, and `mapped_proxy` preserve error propagation across operator chains. Trait structs such as `has_initialize_copy`, `has_initialize_move`, `has_destroy`, `ft_is_complete`, `ft_has_initialize_with_arg`, `variant_index`, `variant_destroyer`, and `variant_visitor` support template dispatch and are public because templates instantiate them from headers.
