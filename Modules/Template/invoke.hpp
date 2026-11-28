#ifndef TEMPLATE_INVOKE_HPP
#define TEMPLATE_INVOKE_HPP

#include <type_traits>
#include <utility>
#include "template_concepts.hpp"

template <typename InstanceType>
constexpr auto ft_invoke_detail_access(InstanceType &&instance) noexcept
    -> std::enable_if_t<std::is_pointer_v<std::decay_t<InstanceType>>,
        decltype(*std::forward<InstanceType>(instance))>
{
    return (*instance);
}

template <typename InstanceType>
constexpr auto ft_invoke_detail_access(InstanceType &&instance) noexcept
    -> std::enable_if_t<!std::is_pointer_v<std::decay_t<InstanceType>>,
        InstanceType &&>
{
    return (std::forward<InstanceType>(instance));
}

template <typename MemberFunction, typename InstanceType, typename... Args>
constexpr auto ft_invoke(MemberFunction &&member_function, InstanceType &&instance, Args&&... args)
#if FT_TEMPLATE_HAS_CONCEPTS
    -> decltype(auto)
    requires std::is_member_function_pointer_v<std::decay_t<MemberFunction>>
#else
    -> typename std::enable_if<
        std::is_member_function_pointer<typename std::decay<MemberFunction>::type>::value,
        decltype((ft_invoke_detail_access(std::forward<InstanceType>(instance)).*
            std::forward<MemberFunction>(member_function))(
                std::forward<Args>(args)...))>::type
#endif
{
    return ((ft_invoke_detail_access(std::forward<InstanceType>(instance)).*
        std::forward<MemberFunction>(member_function))(
            std::forward<Args>(args)...));
}

template <typename MemberObject, typename InstanceType>
constexpr auto ft_invoke(MemberObject &&member_object, InstanceType &&instance)
#if FT_TEMPLATE_HAS_CONCEPTS
    -> decltype(auto)
    requires std::is_member_object_pointer_v<std::decay_t<MemberObject>>
#else
    -> typename std::enable_if<
        std::is_member_object_pointer<typename std::decay<MemberObject>::type>::value,
        decltype(ft_invoke_detail_access(std::forward<InstanceType>(instance)).*
            std::forward<MemberObject>(member_object))>::type
#endif
{
    return (ft_invoke_detail_access(std::forward<InstanceType>(instance)).*
        std::forward<MemberObject>(member_object));
}

template <typename FunctionType, typename... Args>
constexpr auto ft_invoke(FunctionType &&function, Args&&... args)
#if FT_TEMPLATE_HAS_CONCEPTS
    -> decltype(auto)
    requires (!std::is_member_pointer_v<std::decay_t<FunctionType>>)
#else
    -> typename std::enable_if<
        !std::is_member_pointer<typename std::decay<FunctionType>::type>::value,
        decltype(std::forward<FunctionType>(function)(std::forward<Args>(args)...))>::type
#endif
{
    return (std::forward<FunctionType>(function)(std::forward<Args>(args)...));
}

#endif
