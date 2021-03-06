#pragma once

#include <util/type_list.hpp>
#include <util/indexed_tuple.hpp>

#include <array>

#define MBGL_DEFINE_UNIFORM_SCALAR(type_, name_)                                                   \
    struct name_ {                                                                                 \
        using Value = type_;                                                                       \
        static constexpr auto name() {                                                             \
            return #name_;                                                                         \
        }                                                                                          \
    }

#define MBGL_DEFINE_UNIFORM_VECTOR(type_, n_, name_)                                               \
    struct name_ {                                                                                 \
        using Value = std::array<type_, n_>;                                                       \
        static constexpr auto name() {                                                             \
            return #name_;                                                                         \
        }                                                                                          \
    }

#define MBGL_DEFINE_UNIFORM_MATRIX(type_, n_, name_)                                               \
    struct name_ {                                                                                 \
        using Value = std::array<type_, n_ * n_>;                                                  \
        static constexpr auto name() {                                                             \
            return #name_;                                                                         \
        }                                                                                          \
    }

namespace mbgl {
namespace gfx {
namespace detail {

template <class>
class UniformValues;

template <class... Us>
class UniformValues<TypeList<Us...>> {
public:
    using Type = IndexedTuple<TypeList<Us...>, TypeList<typename Us::Value...>>;
};

} // namespace detail

template <class UniformTypeList>
using UniformValues = typename detail::UniformValues<UniformTypeList>::Type;

} // namespace gfx
} // namespace mbgl
