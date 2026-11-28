#include "../test_internal.hpp"
#include "../../Modules/Command/command.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

struct command_test_context
{
    int64_t value;
    int64_t delta;
    int32_t execute_error;
    int32_t undo_error;
    ft_size_t execute_calls;
    ft_size_t undo_calls;
};

static int32_t command_test_execute(void *context)
{
    command_test_context *test_context;

    if (context == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    test_context = static_cast<command_test_context *>(context);
    test_context->execute_calls++;
    if (test_context->execute_error != FT_ERR_SUCCESS)
    {
        return (test_context->execute_error);
    }
    test_context->value = test_context->value + test_context->delta;
    return (FT_ERR_SUCCESS);
}

static int32_t command_test_undo(void *context)
{
    command_test_context *test_context;

    if (context == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    test_context = static_cast<command_test_context *>(context);
    test_context->undo_calls++;
    if (test_context->undo_error != FT_ERR_SUCCESS)
    {
        return (test_context->undo_error);
    }
    test_context->value = test_context->value - test_context->delta;
    return (FT_ERR_SUCCESS);
}

static void command_test_reset_context(command_test_context *context, int64_t delta)
{
    context->value = 0;
    context->delta = delta;
    context->execute_error = FT_ERR_SUCCESS;
    context->undo_error = FT_ERR_SUCCESS;
    context->execute_calls = 0;
    context->undo_calls = 0;
    return ;
}

static command_action command_test_make_action(command_test_context *context,
    const char *name)
{
    command_action action;

    action.name = name;
    action.execute = command_test_execute;
    action.undo = command_test_undo;
    action.context = context;
    return (action);
}

FT_TEST(test_command_stack_initialize_sets_empty_state)
{
    command_action undo_entries[2];
    command_action redo_entries[2];
    command_stack stack;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 2,
            redo_entries, 2));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), command_stack_undo_count(&stack));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), command_stack_redo_count(&stack));
    FT_ASSERT_EQ(FT_FALSE, command_stack_can_undo(&stack));
    FT_ASSERT_EQ(FT_FALSE, command_stack_can_redo(&stack));
    return (1);
}

FT_TEST(test_command_stack_execute_pushes_undo_entry)
{
    command_action undo_entries[2];
    command_action redo_entries[2];
    command_stack stack;
    command_test_context context;
    command_action action;

    command_test_reset_context(&context, 5);
    action = command_test_make_action(&context, "add");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 2,
            redo_entries, 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_execute(&stack, &action));
    FT_ASSERT_EQ(static_cast<int64_t>(5), context.value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), command_stack_undo_count(&stack));
    FT_ASSERT_EQ(FT_TRUE, command_stack_can_undo(&stack));
    return (1);
}

FT_TEST(test_command_stack_undo_moves_entry_to_redo)
{
    command_action undo_entries[2];
    command_action redo_entries[2];
    command_stack stack;
    command_test_context context;
    command_action action;

    command_test_reset_context(&context, 7);
    action = command_test_make_action(&context, "add");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 2,
            redo_entries, 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_execute(&stack, &action));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_undo(&stack));
    FT_ASSERT_EQ(static_cast<int64_t>(0), context.value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), command_stack_undo_count(&stack));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), command_stack_redo_count(&stack));
    return (1);
}

FT_TEST(test_command_stack_redo_moves_entry_to_undo)
{
    command_action undo_entries[2];
    command_action redo_entries[2];
    command_stack stack;
    command_test_context context;
    command_action action;

    command_test_reset_context(&context, 3);
    action = command_test_make_action(&context, "add");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 2,
            redo_entries, 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_execute(&stack, &action));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_undo(&stack));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_redo(&stack));
    FT_ASSERT_EQ(static_cast<int64_t>(3), context.value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), command_stack_undo_count(&stack));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), command_stack_redo_count(&stack));
    return (1);
}

FT_TEST(test_command_stack_execute_clears_redo_history)
{
    command_action undo_entries[3];
    command_action redo_entries[3];
    command_stack stack;
    command_test_context context_one;
    command_test_context context_two;
    command_action action_one;
    command_action action_two;

    command_test_reset_context(&context_one, 1);
    command_test_reset_context(&context_two, 2);
    action_one = command_test_make_action(&context_one, "one");
    action_two = command_test_make_action(&context_two, "two");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 3,
            redo_entries, 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_execute(&stack, &action_one));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_undo(&stack));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), command_stack_redo_count(&stack));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_execute(&stack, &action_two));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), command_stack_redo_count(&stack));
    return (1);
}

FT_TEST(test_command_stack_capacity_drops_oldest_undo_entry)
{
    command_action undo_entries[1];
    command_action redo_entries[1];
    command_stack stack;
    command_test_context context_one;
    command_test_context context_two;
    command_action action_one;
    command_action action_two;
    const command_action *peeked_action;

    command_test_reset_context(&context_one, 1);
    command_test_reset_context(&context_two, 2);
    action_one = command_test_make_action(&context_one, "one");
    action_two = command_test_make_action(&context_two, "two");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 1,
            redo_entries, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_execute(&stack, &action_one));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_execute(&stack, &action_two));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), command_stack_undo_count(&stack));
    peeked_action = command_stack_peek_undo(&stack);
    FT_ASSERT(peeked_action != ft_nullptr);
    FT_ASSERT_EQ(action_two.context, peeked_action->context);
    return (1);
}

FT_TEST(test_command_stack_undo_empty_returns_empty)
{
    command_action undo_entries[1];
    command_action redo_entries[1];
    command_stack stack;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 1,
            redo_entries, 1));
    FT_ASSERT_EQ(FT_ERR_EMPTY, command_stack_undo(&stack));
    return (1);
}

FT_TEST(test_command_stack_overflow_keeps_remaining_actions_in_order)
{
    command_action undo_entries[3];
    command_action redo_entries[2];
    command_stack stack;
    command_test_context contexts[4];
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 3,
            redo_entries, 2));
    ft_size_t index = 0;
    while (index < 4)
    {
        command_test_reset_context(&contexts[index], 1);
        const command_action action = command_test_make_action(&contexts[index], "add");
        FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_execute(&stack, &action));
        ++index;
    }
    while (index > 1)
    {
        --index;
        const command_action *action = command_stack_peek_undo(&stack);
        FT_ASSERT(action != ft_nullptr);
        FT_ASSERT_EQ(static_cast<void *>(&contexts[index]),
            action->context);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_undo(&stack));
    }
    FT_ASSERT_EQ(FT_ERR_EMPTY, command_stack_undo(&stack));
    FT_ASSERT_EQ(static_cast<int64_t>(1), contexts[0].value);
    while (index < 3)
    {
        const command_action *action = command_stack_peek_redo(&stack);
        FT_ASSERT(action != ft_nullptr);
        FT_ASSERT_EQ(static_cast<void *>(&contexts[index]),
            action->context);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_redo(&stack));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_EMPTY, command_stack_redo(&stack));
    FT_ASSERT_EQ(static_cast<int64_t>(0), contexts[3].value);
    return (1);
}

FT_TEST(test_command_stack_redo_empty_returns_empty)
{
    command_action undo_entries[1];
    command_action redo_entries[1];
    command_stack stack;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 1,
            redo_entries, 1));
    FT_ASSERT_EQ(FT_ERR_EMPTY, command_stack_redo(&stack));
    return (1);
}

FT_TEST(test_command_stack_execute_failure_is_not_recorded)
{
    command_action undo_entries[1];
    command_action redo_entries[1];
    command_stack stack;
    command_test_context context;
    command_action action;

    command_test_reset_context(&context, 1);
    context.execute_error = FT_ERR_INVALID_OPERATION;
    action = command_test_make_action(&context, "fail");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 1,
            redo_entries, 1));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, command_stack_execute(&stack, &action));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), command_stack_undo_count(&stack));
    FT_ASSERT_EQ(static_cast<int64_t>(0), context.value);
    return (1);
}

FT_TEST(test_command_stack_undo_failure_restores_undo_entry)
{
    command_action undo_entries[1];
    command_action redo_entries[1];
    command_stack stack;
    command_test_context context;
    command_action action;

    command_test_reset_context(&context, 4);
    action = command_test_make_action(&context, "fail_undo");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 1,
            redo_entries, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_execute(&stack, &action));
    context.undo_error = FT_ERR_INVALID_OPERATION;
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, command_stack_undo(&stack));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), command_stack_undo_count(&stack));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), command_stack_redo_count(&stack));
    return (1);
}

FT_TEST(test_command_stack_initialize_rejects_null_storage_with_capacity)
{
    command_action redo_entries[1];
    command_stack stack;

    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, command_stack_initialize(&stack,
            ft_nullptr, 1, redo_entries, 1));
    return (1);
}

FT_TEST(test_command_stack_execute_rejects_missing_callbacks)
{
    command_action undo_entries[1];
    command_action redo_entries[1];
    command_stack stack;
    command_action action;

    action.name = "invalid";
    action.execute = ft_nullptr;
    action.undo = ft_nullptr;
    action.context = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 1,
            redo_entries, 1));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, command_stack_execute(&stack, &action));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), command_stack_undo_count(&stack));
    return (1);
}

FT_TEST(test_command_stack_execute_reports_full_when_no_undo_capacity)
{
    command_action redo_entries[1];
    command_stack stack;
    command_test_context context;
    command_action action;

    command_test_reset_context(&context, 6);
    action = command_test_make_action(&context, "add");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, ft_nullptr, 0,
            redo_entries, 1));
    FT_ASSERT_EQ(FT_ERR_FULL, command_stack_execute(&stack, &action));
    FT_ASSERT_EQ(static_cast<int64_t>(6), context.value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), command_stack_undo_count(&stack));
    return (1);
}

FT_TEST(test_command_stack_redo_failure_restores_redo_entry)
{
    command_action undo_entries[1];
    command_action redo_entries[1];
    command_stack stack;
    command_test_context context;
    command_action action;

    command_test_reset_context(&context, 4);
    action = command_test_make_action(&context, "fail_redo");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_initialize(&stack, undo_entries, 1,
            redo_entries, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_execute(&stack, &action));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_stack_undo(&stack));
    context.execute_error = FT_ERR_INVALID_OPERATION;
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, command_stack_redo(&stack));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), command_stack_undo_count(&stack));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), command_stack_redo_count(&stack));
    return (1);
}
