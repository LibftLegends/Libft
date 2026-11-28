#include "command.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"

static ft_bool command_action_is_valid(const command_action *action)
{
    if (action == ft_nullptr)
    {
        return (FT_FALSE);
    }
    if (action->execute == ft_nullptr || action->undo == ft_nullptr)
    {
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

static void command_stack_drop_oldest(command_action *entries, ft_size_t count)
{
    if (count > 1)
        (void)ft_memmove(entries, entries + 1, (count - 1) * sizeof(*entries));
    return ;
}

static int32_t command_stack_push(command_action *entries, ft_size_t capacity,
    ft_size_t *count, const command_action *action)
{
    if (count == ft_nullptr || action == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    if (capacity == 0)
    {
        return (FT_ERR_FULL);
    }
    if (entries == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    if (*count >= capacity)
    {
        command_stack_drop_oldest(entries, *count);
        *count = capacity - 1;
    }
    entries[*count] = *action;
    *count = *count + 1;
    return (FT_ERR_SUCCESS);
}

static int32_t command_stack_pop(command_action *entries, ft_size_t *count,
    command_action *action_out)
{
    if (entries == ft_nullptr || count == ft_nullptr || action_out == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    if (*count == 0)
    {
        return (FT_ERR_EMPTY);
    }
    *count = *count - 1;
    *action_out = entries[*count];
    return (FT_ERR_SUCCESS);
}

int32_t command_stack_initialize(command_stack *stack, command_action *undo_entries,
    ft_size_t undo_capacity, command_action *redo_entries, ft_size_t redo_capacity)
{
    if (stack == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    if ((undo_capacity > 0 && undo_entries == ft_nullptr)
        || (redo_capacity > 0 && redo_entries == ft_nullptr))
    {
        return (FT_ERR_INVALID_POINTER);
    }
    stack->undo_entries = undo_entries;
    stack->undo_capacity = undo_capacity;
    stack->undo_count = 0;
    stack->redo_entries = redo_entries;
    stack->redo_capacity = redo_capacity;
    stack->redo_count = 0;
    return (FT_ERR_SUCCESS);
}

void command_stack_clear(command_stack *stack)
{
    if (stack == ft_nullptr)
    {
        return ;
    }
    stack->undo_count = 0;
    stack->redo_count = 0;
    return ;
}

ft_bool command_stack_can_undo(const command_stack *stack)
{
    if (stack == ft_nullptr || stack->undo_count == 0)
    {
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

ft_bool command_stack_can_redo(const command_stack *stack)
{
    if (stack == ft_nullptr || stack->redo_count == 0)
    {
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

ft_size_t command_stack_undo_count(const command_stack *stack)
{
    if (stack == ft_nullptr)
    {
        return (0);
    }
    return (stack->undo_count);
}

ft_size_t command_stack_redo_count(const command_stack *stack)
{
    if (stack == ft_nullptr)
    {
        return (0);
    }
    return (stack->redo_count);
}

int32_t command_stack_execute(command_stack *stack, const command_action *action)
{
    int32_t error_code;

    if (stack == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    if (command_action_is_valid(action) == FT_FALSE)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    error_code = action->execute(action->context);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = command_stack_push(stack->undo_entries, stack->undo_capacity,
            &stack->undo_count, action);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    stack->redo_count = 0;
    return (FT_ERR_SUCCESS);
}

int32_t command_stack_undo(command_stack *stack)
{
    command_action action;
    int32_t error_code;

    if (stack == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    error_code = command_stack_pop(stack->undo_entries, &stack->undo_count,
            &action);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = action.undo(action.context);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)command_stack_push(stack->undo_entries, stack->undo_capacity,
            &stack->undo_count, &action);
        return (error_code);
    }
    return (command_stack_push(stack->redo_entries, stack->redo_capacity,
        &stack->redo_count, &action));
}

int32_t command_stack_redo(command_stack *stack)
{
    command_action action;
    int32_t error_code;

    if (stack == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    error_code = command_stack_pop(stack->redo_entries, &stack->redo_count,
            &action);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = action.execute(action.context);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)command_stack_push(stack->redo_entries, stack->redo_capacity,
            &stack->redo_count, &action);
        return (error_code);
    }
    return (command_stack_push(stack->undo_entries, stack->undo_capacity,
        &stack->undo_count, &action));
}

const command_action *command_stack_peek_undo(const command_stack *stack)
{
    if (stack == ft_nullptr || stack->undo_count == 0)
    {
        return (ft_nullptr);
    }
    return (stack->undo_entries + stack->undo_count - 1);
}

const command_action *command_stack_peek_redo(const command_stack *stack)
{
    if (stack == ft_nullptr || stack->redo_count == 0)
    {
        return (ft_nullptr);
    }
    return (stack->redo_entries + stack->redo_count - 1);
}
