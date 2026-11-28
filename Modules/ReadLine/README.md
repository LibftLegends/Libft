# ReadLine

The `ReadLine` module provides an interactive prompt with history, reverse incremental search, suggestions, key bindings, completion callbacks, editable line state, and pluggable history storage.

## Callback Types

- `t_rl_key_binding_callback` - Custom key handler callback.
- `t_rl_completion_callback` - Completion callback that receives the current buffer, cursor, and prefix.

## History and Suggestions

- `rl_clear_history()` - Removes all in-memory history.
- `rl_add_suggestion(const char *word)` - Adds a suggestion candidate.
- `rl_clear_suggestions()` - Clears suggestion candidates.
- `rl_history_set_backend(...)` - Selects the history backend and location.
- `rl_history_get_backend()` - Returns the active history backend name.
- `rl_history_set_storage_path(...)` - Sets the file path used by persistent history.
- `rl_history_enable_auto_save(ft_bool enabled)` - Controls automatic saves after new commands. Loading saved history leaves the source file unchanged.
- `rl_history_load()` / `rl_history_save()` - Loads or saves history through the configured backend.
- `rl_history_search(...)` - Searches history forward or backward from a start index.
- Reverse incremental search is built into `rl_readline(...)` on `Ctrl-R` and reuses the history search engine to step through earlier matches.

## Key Binding and State Editing

- `rl_bind_key(int32_t key, t_rl_key_binding_callback callback, void *user_data)` - Binds a key to a custom callback.
- `rl_unbind_key(int32_t key)` - Removes a custom key binding.
- `rl_state_insert_text(...)` - Inserts text at the current cursor position. The source may be a suffix borrowed from the current line.
- `rl_state_delete_previous_grapheme(...)` - Deletes the grapheme before the cursor.
- `rl_state_set_cursor(...)` / `rl_state_get_cursor(...)` - Set or read the cursor position.
- `rl_state_get_buffer(...)` - Returns a borrowed pointer to the current line. A later edit can invalidate the pointer.
- `rl_state_refresh_display(...)` - Redraws the prompt and buffer.

## Completion and Prompt

- `rl_set_completion_callback(...)` - Installs a completion provider.
- `rl_completion_add_candidate(const char *candidate)` - Adds one completion candidate.
- Tab completion gives prefix matches a higher base score than fuzzy subsequence matches and breaks score ties alphabetically.
- `rl_readline(const char *prompt)` - Displays a prompt and returns an allocated input line.
