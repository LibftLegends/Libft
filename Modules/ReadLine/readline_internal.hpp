#ifndef READLINE_INTERNAL_HPP
#define READLINE_INTERNAL_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include "../CPP_class/class_file.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <unistd.h>
#include <stdlib.h>

#ifndef DEBUG
# define DEBUG 0
#endif

#define MAX_HISTORY 1000
#define MAX_SUGGESTIONS 1000
#define INITIAL_BUFFER_SIZE 1024
#define RL_KEY_CTRL_R 18

extern char *history[MAX_HISTORY];
extern int32_t history_count;

// Static candidates are stored in bytewise lexical order.
extern char *suggestions[MAX_SUGGESTIONS];
extern int32_t suggestion_count;

class pt_recursive_mutex;

struct s_readline_state
{
    s_readline_state(void);
    ~s_readline_state(void);

    char        *buffer;
    int32_t            buffer_size;
    int32_t            position;
    int32_t            prev_buffer_length;
    int32_t            display_pos;
    int32_t            prev_display_columns;
    int32_t            history_index;
    int32_t            in_completion_mode;
    int32_t            current_match_count;
    int32_t            current_match_index;
    int32_t            word_start;
    // Only entries below current_match_count are initialized and available.
    char        *current_matches[MAX_SUGGESTIONS];
    int32_t            current_match_scores[MAX_SUGGESTIONS];
    ft_file        error_file;
    pt_recursive_mutex    *mutex;
};

typedef struct s_readline_state readline_state_t;

typedef int32_t (*t_rl_key_binding_callback)(readline_state_t *state, const char *prompt, void *user_data);

typedef int32_t (*t_rl_completion_callback)(const char *buffer, int32_t cursor_position,
    const char *prefix, void *user_data);

int32_t        rl_initialize_state(readline_state_t *state);

int32_t        rl_disable_raw_mode();
int32_t        rl_enable_raw_mode(ft_bool *activated = ft_nullptr);

int32_t        rl_clear_line(const char *prompt, const char *buffer);
int32_t        rl_clear_line_columns(const char *prompt, int32_t buffer_columns);
int32_t        rl_resize_buffer(char **buffer_pointer, int32_t *current_size_pointer, int32_t new_size);
int32_t        rl_handle_escape_sequence(readline_state_t *state, const char *prompt);
int32_t        rl_handle_backspace(readline_state_t *state, const char *prompt);
int32_t        rl_handle_tab_completion(readline_state_t *state, const char *prompt);
int32_t        rl_handle_printable_char(readline_state_t *state, char character, const char *prompt);
int32_t        rl_handle_reverse_history_search_key(readline_state_t *state, const char *prompt, void *user_data);
int32_t        rl_update_display_metrics(readline_state_t *state);
int32_t        rl_utf8_compute_columns(const char *string, int32_t *columns);
int32_t        rl_utf8_find_previous_grapheme(const char *buffer, int32_t cursor_pos,
            int32_t *start_byte, int32_t *end_byte, int32_t *display_width);
int32_t        rl_utf8_find_next_grapheme(const char *buffer, int32_t cursor_pos,
            int32_t *start_byte, int32_t *end_byte, int32_t *display_width);

int32_t        rl_read_key(char *character_out);
int32_t        rl_get_terminal_width(int32_t *terminal_width);
int32_t        rl_read_escape_sequence(char escape_sequence[2]);
void    rl_update_history(const char *buffer);
// Takes ownership of a CMA-allocated entry without scheduling a save.
void    rl_history_adopt_entry(char *entry);
void    rl_reset_completion_mode(readline_state_t *state);

int32_t        rl_dispatch_custom_key(readline_state_t *state, const char *prompt, int32_t key, ft_bool *key_handled);

int32_t        rl_bind_key(int32_t key, t_rl_key_binding_callback callback, void *user_data);
int32_t        rl_unbind_key(int32_t key);

int32_t        rl_state_insert_text(readline_state_t *state, const char *text);
int32_t        rl_state_delete_previous_grapheme(readline_state_t *state);
int32_t        rl_state_set_cursor(readline_state_t *state, int32_t new_position);
int32_t        rl_state_get_cursor(readline_state_t *state, int32_t *out_position);
int32_t        rl_state_get_buffer(readline_state_t *state, const char **out_buffer);
int32_t        rl_state_refresh_display(readline_state_t *state, const char *prompt);

int32_t        rl_set_completion_callback(t_rl_completion_callback callback, void *user_data);
int32_t        rl_completion_add_candidate(const char *candidate);
void    rl_completion_reset_dynamic_matches(void);
int32_t        rl_completion_prepare_candidates(const char *buffer, int32_t cursor_position,
            const char *prefix, int32_t prefix_length);
int32_t        rl_completion_get_dynamic_count(int32_t *count_out);
char    *rl_completion_get_dynamic_match(int32_t index);

int32_t        rl_history_set_backend(const char *backend_name, const char *location);
const char    *rl_history_get_backend(void);
int32_t        rl_history_set_storage_path(const char *file_path);
int32_t        rl_history_enable_auto_save(ft_bool enabled);
int32_t        rl_history_load(void);
int32_t        rl_history_save(void);
void    rl_history_notify_updated(void);
int32_t        rl_history_search(const char *query, int32_t start_index,
            ft_bool search_backward, int32_t *match_index);

int32_t        rl_state_prepare_thread_safety(readline_state_t *state);
void    rl_state_teardown_thread_safety(readline_state_t *state);
int32_t        rl_state_lock(readline_state_t *state, ft_bool *lock_acquired);
int32_t        rl_state_unlock(readline_state_t *state, ft_bool lock_acquired);

#endif
