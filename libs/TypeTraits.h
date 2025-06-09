/*
 * File:   TypeTraits.h
 * Author: thaipq
 *
 * Created on Mon Jun 09 2025 11:10:06 AM
 */

#ifndef LIBS_TYPETRAITS_H
#define LIBS_TYPETRAITS_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <type_traits>

namespace libs {
    using enabler = void;

    template<typename T>
    struct is_number : std::is_arithmetic<T> {};

    template<typename T>
    struct is_string : std::false_type {};
    template<>
    struct is_string<std::string> : std::true_type {};

    template<typename T>
    struct is_enum : std::is_enum<T> {};

    template<typename T, typename S = std::ostringstream>
    class is_ostreamable {
        template<typename TT, typename SS>
        static auto test(int) -> decltype(std::declval<SS &>() << std::declval<TT>(), std::true_type());

        template<typename, typename>
        static auto test(...) -> std::false_type;

    public:
        static constexpr bool value = decltype(test<T, S>(0))::value;
    };

#if __cplusplus < 202002L
    template<typename T>
    struct remove_cvref {
        using type = typename std::remove_cv<
                typename std::remove_reference<T>::type>::type;
    };
    template<typename T>
    using remove_cvref_t = typename remove_cvref<T>::type;
#else
    template<typename T>
    using remove_cvref = std::remove_cvref<T>;
    template<typename T>
    using remove_cvref_t = std::remove_cvref_t<T>;
#endif

    template<typename T>
    struct is_std_vector_impl : std::false_type {};
    template<typename T, typename Alloc>
    struct is_std_vector_impl<std::vector<T, Alloc>> : std::true_type {};
    template<typename T>
    using is_std_vector = is_std_vector_impl<remove_cvref_t<T>>;


    template<typename T>
    struct is_std_pair_impl : std::false_type {};
    template<typename T1, typename T2>
    struct is_std_pair_impl<std::pair<T1, T2>> : std::true_type {};
    template<typename T>
    using is_std_pair = is_std_pair_impl<remove_cvref_t<T>>;

    template<class T>
    struct is_std_map_impl : std::false_type {};
    template<class Key, class Val, class Cmp, class Alloc>
    struct is_std_map_impl<std::map<Key, Val, Cmp, Alloc>> : std::true_type {};
    template<typename T>
    using is_std_map = is_std_map_impl<remove_cvref_t<T>>;

    template<class T>
    struct is_std_set_impl : std::false_type {};
    template<class Key, class Cmp, class Alloc>
    struct is_std_set_impl<std::set<Key, Cmp, Alloc>> : std::true_type {};
    template<typename T>
    using is_std_set = is_std_set_impl<remove_cvref_t<T>>;

}// namespace libs

#endif// LIBS_TYPETRAITS_H
