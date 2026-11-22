# Standalone performance checks

- `benchmark_api_connection_pool.cpp` measures returning and reacquiring an idle TCP connection on loopback.
- `benchmark_api_http_common.cpp` measures CRLF search on an 8 KiB line, decoding a 64 KiB chunked body, and measuring and streaming a 128-field JSON request.

- `benchmark_analytics.cpp` measures region statistics, sampled frame publication, frame completion with 128 trace events across one and 64 regions, JSON and CSV serialization, and draining trace events to a buffered JSONL file.

- `benchmark_json_schema_evolution.cpp` measures latest-version lookup and 16-step migration routes in registries with 32 and 1,024 transitions.
- `benchmark_file_io.cpp` measures mixed-separator path joins and repeated reads of an 8 MiB binary file into the same string.
- `benchmark_file_atomic_write.cpp` measures parent path extraction and repeated atomic replacements of a small save file.
- `benchmark_file_traversal.cpp` measures recursive directory listing and callback traversal over a save-style directory tree.
- `benchmark_filesystem_paths.cpp` measures path splitting, a single-star match, and a multi-star near miss.
- `benchmark_filesystem_temp_join.cpp` measures temporary-path generation and safe joins under an absolute root.
- `benchmark_file_hash_metadata.cpp` measures both file digests over 8 MiB and metadata comparisons between regular files.
- `benchmark_file_tree_operations.cpp` measures repeated directory creation, recursive copying, and recursive deletion on a save-style tree.
- `benchmark_file_relative_path.cpp` measures relative path calculation across sibling directories with drive-letter and case differences.
- `benchmark_storage_lookups.cpp` measures updates and lookups of existing keys in a 1,024-entry store, with and without a pending expiration, and two-key update batches.
- `benchmark_storage_snapshots.cpp` measures export, flush, reload, move, and background-worker shutdown for a 2,048-entry JSON store with expiration metadata. It also measures pruning 128 expired keys and encrypted in-memory snapshots of a 64 KiB payload.
- `benchmark_storage_formats.cpp` measures JSON Lines, memory-mapped, and optional SQLite flushes for a 512-entry store with expiration metadata.

- `benchmark_basic_memory.cpp` measures late byte searches, overlapping moves in both directions, equal and late-mismatch comparisons, small and large copies, and binary substring searches.
- `benchmark_basic_text.cpp` measures string length, character and substring searches, and UTF-8 validation and counting.
- `benchmark_basic_parsing.cpp` measures integer parsing and validation, tokenizing, and bounded string copies.
- `benchmark_allocator_paths.cpp` measures CMA resize and reuse with and without other live allocations, plus single-request reads and writes, fragmented allocation, out-of-order compaction, and tail resize in SCMA.
- `benchmark_buffer_transfers.cpp` measures self-appending and slicing in a byte buffer, plus typed SCMA reads and writes.
- `benchmark_advanced_text.cpp` measures string duplication, prefix substrings, variadic joins, and UTF-8 and UTF-16 conversion.
- `benchmark_encoding.cpp` measures binary Base64, Base32, hex, and percent round trips, plus ASCII percent conversion and UTF-8 validation.
- `benchmark_compression.cpp` measures the Compression Base64 codec with plain and wrapped data, plus the zlib string-to-vector adapter.
- `benchmark_errno.cpp` measures error-code lookup and prefixed reports written to an unbuffered null device.
- `benchmark_time.cpp` measures monotonic clock reads, RFC 3339 parsing, and formatting for date fields, timestamps, relative durations, and intervals.
- `benchmark_time_runtime.cpp` measures calendar rounding, clock translation, frame pacing, and trace event writes to a null device.
- `benchmark_pthread_wait.cpp` measures nested recursive mutex operations and atomic wake-and-wait round trips between two threads.
- `benchmark_pthread_sync.cpp` measures reader-lock bookkeeping, condition signaling, and Windows timed joins.
- `benchmark_threading.cpp` measures single-worker task submission and completion, plus scheduler tracing with zero or one sink.
- `benchmark_template_sort.cpp` measures sort time and comparison count for seeded integer ranges.
- `benchmark_template_containers.cpp` measures circular buffer push/pop, vector reservation with live integers, and priority queue push/pop with 64-byte entries.
- `benchmark_container_serialization.cpp` measures integer vector serialization to a JSON group.
- `benchmark_linked_containers.cpp` measures queue, stack, and deque throughput with alternating insertion and removal.
- `benchmark_string_view.cpp` measures comparisons of equal 16 KiB views and views with a late mismatch.
- `benchmark_template_callbacks.cpp` measures by-value callback dispatch and listener removal from the front of a 4,096-listener emitter.
- `benchmark_future_wait.cpp` measures the interval from publishing a promise to completion of a waiting future.
- `benchmark_shared_ptr.cpp` measures assignment between wrappers that already share ownership.
- `benchmark_unordered_map.cpp` measures erase and reinsert in a large collision cluster that wraps around the table.
- `benchmark_ordered_set.cpp` measures insertion and removal at the front of a sorted set of 8,192 integers.
- `benchmark_map_lookup.cpp` measures mixed hits and misses in insertion-ordered maps of 8 and 4,096 integer keys.
- `benchmark_result_failure.cpp` measures creating and discarding an error result with room for a 64 KiB value.
- `benchmark_template_value_access.cpp` measures reading stored optional, variant, and tuple values and counts fallback constructions.
- `benchmark_matrix.cpp` measures rectangular double-precision matrix multiplication.
- `benchmark_graph_traversal.cpp` measures paired breadth-first and depth-first traversals of a 64-vertex graph.
- `benchmark_pool_trie.cpp` measures incremental pool growth and the build and lookup costs of a trie with all 255 nonzero leading bytes.
- `benchmark_json_io.cpp` measures legacy JSON parsing and cleanup, plus streaming string writes and callback counts.
- `benchmark_json_file.cpp` measures writing an 8,192-item group into a temporary directory created by the benchmark.
- `benchmark_json_stream_reader.cpp` measures streaming event traversal over short and long strings, digit runs, and whitespace.
- `benchmark_json_schema.cpp` measures validating numeric schemas of 8 to 1,024 fields.
- `benchmark_json_item_updates.cpp` measures repeated and alternating updates to the last item in a JSON group.
- `benchmark_json_serializer.cpp` measures the string output stage for a document with a 1 MiB value and counts CMA allocations.
- `benchmark_json_document.cpp` measures plain and escaped JSON Pointer lookups plus document copying.
- `benchmark_json_dom.cpp` measures decimal big-number formatting and JSON-to-DOM round trips.
- `benchmark_math_fft.cpp` measures repeated 65,536-point transforms of an impulse.
- `benchmark_voxel_mesh.cpp` measures chunk copies, frustum culling, and mesh generation for a checkerboard of solid blocks.
- `benchmark_game_voxel_save.cpp` times the terrain codec, temporary file creation, writes, and atomic replacement separately. Run it from an empty working directory reserved for the benchmark.

- `benchmark_api_http_plain.cpp` measures streaming chunk decoding for a response with 1,024 small payloads and a trailer.
- `benchmark_api_http_chunk_scan.cpp` compares rechecking the entire body with resuming from the previous read for a chunked response delivered in 256-byte reads.
- `benchmark_http2_frame_codec.cpp` measures encoding and decoding a 16 KiB HTTP/2 frame in an OpenSSL-enabled build.
- `benchmark_http2_header_codec.cpp` measures serializing and parsing 24 header fields with 1 KiB values in an OpenSSL-enabled build.
- `benchmark_http2_stream_append.cpp` measures appending a 16 KiB payload to a fresh stream and updating receive windows.
- `benchmark_http2_stream_windows.cpp` measures changing the initial receive window with 4,096 active streams.
- `benchmark_api_connection_pool_key.cpp` measures building a connection key on an existing handle with the pool disabled.
- `benchmark_api_retry_circuit.cpp` measures successful circuit checks with 1,024 other destinations holding recorded failures.
- `benchmark_api_retry_policy.cpp` measures one policy write and five reads with optional thread safety disabled.
- `benchmark_networking_metrics_dispatch.cpp` measures forwarding samples to an exporter from four concurrent threads.
- `benchmark_networking_dns_cache.cpp` measures cached lookups of the first address with one and 1,024 host and service entries.

Each source is a separate executable. From the Libft root, link it against a freshly built archive for your platform and configuration. This MinGW example uses the archive from a Windows voxel build. Set `$archive` to the path printed by your build and use the corresponding configuration flags.

```powershell
$archive = 'build/archives/Full_Libft_ft_vox_cfg3075396341.a'
g++ -std=c++17 -O2 -DLIBFT_INTERNAL_HEADERS -DGAME_USE_VOXEL_REGION_BACKEND `
    Test/Efficiency/Standalone/benchmark_basic_text.cpp `
    $archive -lws2_32 -ldbghelp -lz -lwinmm -o build/basic-text-bench.exe
./build/basic-text-bench.exe
```

For a comparison, build the same benchmark source against each revision's headers and archive. Use the same compiler flags and fixture, then run each executable several times on the same machine. Use the regular Libft tests for pass or fail results.
