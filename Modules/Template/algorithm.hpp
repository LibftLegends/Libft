#ifndef FT_ALGORITHM_HPP
#define FT_ALGORITHM_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <type_traits>
#include "swap.hpp"
#include "../RNG/rng.hpp"

template <typename IteratorType, typename = void>
struct ft_sort_has_random_access_category : std::false_type
{
};

template <typename IteratorType>
struct ft_sort_has_random_access_category<IteratorType,
    std::void_t<typename std::iterator_traits<IteratorType>::iterator_category>>
    : std::is_base_of<std::random_access_iterator_tag,
        typename std::iterator_traits<IteratorType>::iterator_category>
{
};

template <typename RandomIt, typename Compare>
void ft_sort(RandomIt first, RandomIt last, Compare compare)
{
    if (!(first != last))
        return ;
    if constexpr (ft_sort_has_random_access_category<RandomIt>::value)
    {
        std::sort(first, last, std::ref(compare));
    }
    else
    {
        // Keep accepting iterators that only provide the original operations.
        RandomIt position = first;
        while (position != last)
        {
            RandomIt candidate = position + 1;
            while (candidate != last)
            {
                if (compare(*candidate, *position))
                    ft_swap(*position, *candidate);
                ++candidate;
            }
            ++position;
        }
    }
    return ;
}

template <typename RandomIt>
void ft_sort(RandomIt first, RandomIt last)
{
    ft_sort(first, last, [](const auto& a, const auto& b)
    {
        return (a < b);
    });
    return ;
}

template <typename RandomIt, typename T, typename Compare>
constexpr ft_bool ft_binary_search(RandomIt first, RandomIt last, const T& value, Compare compare)
{
    while (first < last)
    {
        RandomIt middle = first + (last - first) / 2;
        if (compare(*middle, value))
            first = middle + 1;
        else if (compare(value, *middle))
            last = middle;
        else
            return (FT_TRUE);
    }
    return (FT_FALSE);
}

template <typename RandomIt, typename T>
constexpr ft_bool ft_binary_search(RandomIt first, RandomIt last, const T& value)
{
    return (ft_binary_search(first, last, value,
        [](const auto& a, const auto& b)
        {
            return (a < b);
        }));
}

template <typename RandomIt>
void ft_shuffle(RandomIt first, RandomIt last)
{
    if (first == last)
        return ;
    RandomIt iterator = last - 1;
    while (iterator > first)
    {
        ft_size_t random_index = static_cast<ft_size_t>(ft_random_int()) %
                    static_cast<ft_size_t>((iterator - first) + 1);
        ft_swap(*(first + random_index), *iterator);
        --iterator;
    }
    return ;
}

template <typename BidirectionalIt>
constexpr void ft_reverse(BidirectionalIt first, BidirectionalIt last)
{
    while (first != last && first != --last)
    {
        ft_swap(*first, *last);
        ++first;
    }
    return ;
}

#endif 
