#include "pair.hpp"
#include <cstdint>
#include "queue.hpp"
#include "stack.hpp"
#include "vector.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

[[maybe_unused]] static int32_t build_types()
{
    ft_vector<int32_t> integers;

    integers.push_back(3);
    integers.push_back(7);
    if (!integers.empty())
        integers.pop_back();
    Pair<int32_t, int64_t> pair_value(42, 13);
    ft_vector<Pair<int32_t, int64_t>> pair_collection;
    pair_collection.push_back(pair_value);
    ft_queue<int32_t> queue_values;
    queue_values.enqueue(5);
    queue_values.enqueue(10);
    if (!queue_values.empty())
        queue_values.dequeue();
    ft_stack<double> stack_values;
    stack_values.push(1.5);
    stack_values.push(2.5);
    if (!stack_values.empty())
        stack_values.pop();
    int32_t queue_head = queue_values.front();
    double stack_top = stack_values.top();
    (void)queue_head;
    (void)stack_top;
    return (static_cast<int32_t>(integers.size()
            + pair_collection.size()
            + queue_values.size()
            + stack_values.size()));
}

