#ifndef FT_GRAPH_HPP
#define FT_GRAPH_HPP

#include "constructor.hpp"
#include "move.hpp"
#include "vector.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Printf/printf.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

template <typename VertexType>
class ft_graph
{
    private:
        struct graph_node
        {
            VertexType *_value_pointer;
            ft_size_t   *_edges;
            ft_size_t    _degree;
            ft_size_t    _edge_capacity;
        };

        struct traversal_frame
        {
            ft_size_t _vertex;
            ft_size_t _next_edge;
        };

        graph_node               *_nodes;
        ft_size_t                 _node_capacity;
        ft_size_t                 _size;
        ft_size_t                 _initial_capacity;
        mutable pt_recursive_mutex *_mutex;
        uint8_t                   _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        void reset_node(graph_node &node);
        void destroy_node(graph_node &node);
        ft_bool ensure_node_capacity(ft_size_t desired_capacity);
        ft_bool ensure_edge_capacity(graph_node &node, ft_size_t desired_capacity);
        void destroy_all_nodes_unlocked();
        template <typename WorkItem>
        static WorkItem *allocate_traversal(ft_size_t count, ft_bool *&visited);

    public:
        explicit ft_graph(ft_size_t initial_capacity = 0);
        ft_graph(const ft_graph<VertexType> &other) = delete;
        ft_graph(ft_graph<VertexType> &&other) = delete;
        ~ft_graph();
        ft_graph& operator=(const ft_graph&) = delete;
        ft_graph& operator=(ft_graph&& other) = delete;

        int32_t initialize();
        int32_t destroy();
        int32_t move(ft_graph<VertexType> &other);

        ft_size_t add_vertex(const VertexType& value);
        ft_size_t add_vertex(VertexType&& value);
        void add_edge(ft_size_t from, ft_size_t to);

        template <typename Func>
        void bfs(ft_size_t start, Func visit);

        template <typename Func>
        void dfs(ft_size_t start, Func visit);

        void neighbors(ft_size_t index, ft_vector<ft_size_t> &out) const;
        ft_size_t size() const;
        ft_bool empty() const;
        void clear();

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename VertexType>
thread_local int32_t ft_graph<VertexType>::_last_error = FT_ERR_SUCCESS;

template <typename VertexType>
int32_t ft_graph<VertexType>::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

template <typename VertexType>
int32_t ft_graph<VertexType>::lock_internal(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    lock_result = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_result != FT_ERR_SUCCESS)
        return (set_error(lock_result));
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

template <typename VertexType>
int32_t ft_graph<VertexType>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename VertexType>
void ft_graph<VertexType>::reset_node(graph_node &node)
{
    node._value_pointer = ft_nullptr;
    node._edges = ft_nullptr;
    node._degree = 0;
    node._edge_capacity = 0;
    return ;
}

template <typename VertexType>
void ft_graph<VertexType>::destroy_node(graph_node &node)
{
    if (node._value_pointer != ft_nullptr)
    {
        destroy_at(node._value_pointer);
        cma_free(node._value_pointer);
    }
    if (node._edges != ft_nullptr)
        cma_free(node._edges);
    this->reset_node(node);
    return ;
}

template <typename VertexType>
ft_bool ft_graph<VertexType>::ensure_node_capacity(ft_size_t desired_capacity)
{
    if (desired_capacity <= this->_node_capacity)
        return (FT_TRUE);
    constexpr ft_size_t MAXIMUM_CAPACITY = FT_SYSTEM_SIZE_MAX / sizeof(graph_node);
    if (desired_capacity > MAXIMUM_CAPACITY)
    {
        set_error(FT_ERR_NO_MEMORY);
        return (FT_FALSE);
    }
    ft_size_t new_capacity = desired_capacity;
    if (this->_node_capacity <= MAXIMUM_CAPACITY / 2U
        && this->_node_capacity * 2U > desired_capacity)
        new_capacity = this->_node_capacity * 2U;
    graph_node *new_nodes = static_cast<graph_node*>(cma_malloc(sizeof(graph_node) * new_capacity));
    if (new_nodes == ft_nullptr)
    {
        set_error(FT_ERR_NO_MEMORY);
        return (FT_FALSE);
    }
    if (this->_size != 0U)
        std::uninitialized_copy_n(this->_nodes, this->_size, new_nodes);
    if (this->_nodes != ft_nullptr)
        cma_free(this->_nodes);
    this->_nodes = new_nodes;
    this->_node_capacity = new_capacity;
    set_error(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

template <typename VertexType>
ft_bool ft_graph<VertexType>::ensure_edge_capacity(graph_node &node,
    ft_size_t desired_capacity)
{
    if (desired_capacity <= node._edge_capacity)
        return (FT_TRUE);
    constexpr ft_size_t MAXIMUM_CAPACITY = FT_SYSTEM_SIZE_MAX / sizeof(ft_size_t);
    if (desired_capacity > MAXIMUM_CAPACITY)
    {
        set_error(FT_ERR_NO_MEMORY);
        return (FT_FALSE);
    }
    ft_size_t new_capacity = desired_capacity;
    if (node._edge_capacity <= MAXIMUM_CAPACITY / 2U
        && node._edge_capacity * 2U > desired_capacity)
        new_capacity = node._edge_capacity * 2U;
    ft_size_t *new_edges = static_cast<ft_size_t*>(cma_malloc(sizeof(ft_size_t) * new_capacity));
    if (new_edges == ft_nullptr)
    {
        set_error(FT_ERR_NO_MEMORY);
        return (FT_FALSE);
    }
    if (node._degree != 0U)
        std::uninitialized_copy_n(node._edges, node._degree, new_edges);
    if (node._edges != ft_nullptr)
        cma_free(node._edges);
    node._edges = new_edges;
    node._edge_capacity = new_capacity;
    set_error(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

template <typename VertexType>
void ft_graph<VertexType>::destroy_all_nodes_unlocked()
{
    ft_size_t index;

    index = 0;
    while (index < this->_size)
    {
        this->destroy_node(this->_nodes[index]);
        ++index;
    }
    this->_size = 0;
    return ;
}

template <typename VertexType>
ft_graph<VertexType>::ft_graph(ft_size_t initial_capacity)
    : _nodes(ft_nullptr)
    , _node_capacity(0)
    , _size(0)
    , _initial_capacity(initial_capacity)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename VertexType>
ft_graph<VertexType>::~ft_graph()
{
    uint32_t previous_error;

    previous_error = ft_graph<VertexType>::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

template <typename VertexType>
int32_t ft_graph<VertexType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_graph::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_nodes = ft_nullptr;
    this->_node_capacity = 0;
    this->_size = 0;
    if (this->ensure_node_capacity(this->_initial_capacity) == FT_FALSE)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_NO_MEMORY));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename VertexType>
int32_t ft_graph<VertexType>::destroy()
{
    int32_t first_error;
    int32_t disable_error;

    first_error = FT_ERR_SUCCESS;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        first_error = disable_error;
    this->destroy_all_nodes_unlocked();
    if (this->_nodes != ft_nullptr)
    {
        cma_free(this->_nodes);
        this->_nodes = ft_nullptr;
    }
    this->_node_capacity = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template <typename VertexType>
int32_t ft_graph<VertexType>::move(ft_graph<VertexType> &other)
{
    int32_t destroy_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_graph::move",
            "source object is uninitialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
    }
    this->_initial_capacity = other._initial_capacity;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_nodes = ft_nullptr;
        this->_node_capacity = 0;
        this->_size = 0;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_nodes = other._nodes;
    this->_node_capacity = other._node_capacity;
    this->_size = other._size;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._nodes = ft_nullptr;
    other._node_capacity = 0;
    other._size = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename VertexType>
ft_size_t ft_graph<VertexType>::add_vertex(const VertexType& value)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    VertexType *new_value;
    ft_size_t index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::add_vertex(copy)");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (this->_size);
    }
    if (this->ensure_node_capacity(this->_size + 1) == FT_FALSE)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return (this->_size);
    }
    new_value = static_cast<VertexType*>(cma_malloc(sizeof(VertexType)));
    if (new_value == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return (this->_size);
    }
    construct_at(new_value, value);
    index = this->_size;
    ::construct_at(this->_nodes + index, graph_node{new_value, ft_nullptr, 0U, 0U});
    this->_size += 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (index);
}

template <typename VertexType>
ft_size_t ft_graph<VertexType>::add_vertex(VertexType&& value)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    VertexType *new_value;
    ft_size_t index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::add_vertex(move)");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (this->_size);
    }
    if (this->ensure_node_capacity(this->_size + 1) == FT_FALSE)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return (this->_size);
    }
    new_value = static_cast<VertexType*>(cma_malloc(sizeof(VertexType)));
    if (new_value == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return (this->_size);
    }
    construct_at(new_value, ft_move(value));
    index = this->_size;
    ::construct_at(this->_nodes + index, graph_node{new_value, ft_nullptr, 0U, 0U});
    this->_size += 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (index);
}

template <typename VertexType>
void ft_graph<VertexType>::add_edge(ft_size_t from, ft_size_t to)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    graph_node *node;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::add_edge");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    if (from >= this->_size || to >= this->_size)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NOT_FOUND);
        return ;
    }
    node = &this->_nodes[from];
    if (this->ensure_edge_capacity(*node, node->_degree + 1) == FT_FALSE)
    {
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    ::construct_at(node->_edges + node->_degree, to);
    node->_degree += 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename VertexType>
template <typename WorkItem>
WorkItem *ft_graph<VertexType>::allocate_traversal(ft_size_t count, ft_bool *&visited)
{
    constexpr ft_size_t BYTES_PER_VERTEX = sizeof(WorkItem) + sizeof(ft_bool);
    if (count > FT_SYSTEM_SIZE_MAX / BYTES_PER_VERTEX)
        return (ft_nullptr);
    void *storage = cma_malloc(count * BYTES_PER_VERTEX);
    if (storage == ft_nullptr)
        return (ft_nullptr);
    visited = reinterpret_cast<ft_bool *>(static_cast<unsigned char *>(storage)
        + count * sizeof(WorkItem));
    std::uninitialized_fill_n(visited, count, FT_FALSE);
    return (static_cast<WorkItem *>(storage));
}

template <typename VertexType>
template <typename Func>
void ft_graph<VertexType>::bfs(ft_size_t start, Func visit)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::bfs");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    const ft_size_t vertex_count = this->_size;
    if (start >= vertex_count)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NOT_FOUND);
        return ;
    }
    ft_bool *visited;
    ft_size_t *queue = allocate_traversal<ft_size_t>(vertex_count, visited);
    if (queue == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return ;
    }
    ft_size_t queue_head = 0U;
    ft_size_t queue_tail = 1U;
    visited[start] = FT_TRUE;
    ::construct_at(queue, start);
    while (queue_head < queue_tail)
    {
        const ft_size_t current = queue[queue_head++];
        if (current >= this->_size)
            continue ;
        visit(*this->_nodes[current]._value_pointer);
        if (current >= this->_size)
            continue ;
        const graph_node &node = this->_nodes[current];
        ft_size_t edge_index = 0U;
        while (edge_index < node._degree)
        {
            const ft_size_t adjacent = node._edges[edge_index++];
            if (adjacent < vertex_count && visited[adjacent] == FT_FALSE)
            {
                visited[adjacent] = FT_TRUE;
                ::construct_at(queue + queue_tail, adjacent);
                ++queue_tail;
            }
        }
    }
    cma_free(queue);
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename VertexType>
template <typename Func>
void ft_graph<VertexType>::dfs(ft_size_t start, Func visit)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::dfs");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    const ft_size_t vertex_count = this->_size;
    if (start >= vertex_count)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NOT_FOUND);
        return ;
    }
    ft_bool *visited;
    traversal_frame *stack = allocate_traversal<traversal_frame>(vertex_count, visited);
    if (stack == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return ;
    }
    // A frame resumes at the next edge after its child returns.
    ft_size_t stack_size = 1U;
    visited[start] = FT_TRUE;
    ::construct_at(stack, traversal_frame{start, 0U});
    visit(*this->_nodes[start]._value_pointer);
    while (stack_size != 0U)
    {
        traversal_frame &frame = stack[stack_size - 1U];
        if (frame._vertex >= this->_size
            || frame._next_edge >= this->_nodes[frame._vertex]._degree)
        {
            --stack_size;
            continue ;
        }
        const ft_size_t adjacent = this->_nodes[frame._vertex]._edges[frame._next_edge++];
        if (adjacent >= vertex_count || visited[adjacent] != FT_FALSE)
            continue ;
        visited[adjacent] = FT_TRUE;
        ::construct_at(stack + stack_size, traversal_frame{adjacent, 0U});
        ++stack_size;
        visit(*this->_nodes[adjacent]._value_pointer);
    }
    cma_free(stack);
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename VertexType>
void ft_graph<VertexType>::neighbors(ft_size_t index,
    ft_vector<ft_size_t> &out) const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t edge_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::neighbors");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    if (index >= this->_size)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NOT_FOUND);
        return ;
    }
    edge_index = 0;
    while (edge_index < this->_nodes[index]._degree)
    {
        out.push_back(this->_nodes[index]._edges[edge_index]);
        ++edge_index;
    }
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename VertexType>
ft_size_t ft_graph<VertexType>::size() const
{
    ft_size_t value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::size");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (0);
    value = this->_size;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (value);
}

template <typename VertexType>
ft_bool ft_graph<VertexType>::empty() const
{
    ft_bool value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::empty");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_TRUE);
    value = (this->_size == 0);
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (value);
}

template <typename VertexType>
void ft_graph<VertexType>::clear()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::clear");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    this->destroy_all_nodes_unlocked();
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename VertexType>
int32_t ft_graph<VertexType>::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
        return (set_error(FT_ERR_NO_MEMORY));
    initialize_result = new_mutex->initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        return (set_error(initialize_result));
    }
    this->_mutex = new_mutex;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename VertexType>
int32_t ft_graph<VertexType>::disable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        && this->_initialised_state != FT_CLASS_STATE_DESTROYED)
        return (set_error(FT_ERR_INVALID_STATE));
    mutex_pointer = this->_mutex;
    if (mutex_pointer == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    this->_mutex = ft_nullptr;
    destroy_result = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_result != FT_ERR_SUCCESS)
        return (set_error(destroy_result));
    return (set_error(FT_ERR_SUCCESS));
}

template <typename VertexType>
ft_bool ft_graph<VertexType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename VertexType>
int32_t ft_graph<VertexType>::lock(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_graph::lock");
    lock_result = this->lock_internal(lock_acquired);
    return (set_error(lock_result));
}

template <typename VertexType>
void ft_graph<VertexType>::unlock(ft_bool lock_acquired) const
{
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename VertexType>
int32_t ft_graph<VertexType>::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_graph::get_error");
    return (_last_error);
}

template <typename VertexType>
const char *ft_graph<VertexType>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_graph::get_error_str");
    return (ft_strerror(_last_error));
}

#endif
