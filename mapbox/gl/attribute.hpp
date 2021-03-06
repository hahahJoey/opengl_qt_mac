#pragma once

#include <gl/types.hpp>
#include <util/optional.hpp>
#include <gl/vertex_buffer.hpp>
#include <util/type_list.hpp>
#include <util/indexed_tuple.hpp>
#include <gl/gfx/attribute.hpp>

#include <type_traits>
#include <vector>
#include <set>
#include <string>

namespace mbgl {
namespace gl {

template<class> struct DataTypeOf;
template<> struct DataTypeOf<int8_t> : std::integral_constant<DataType, DataType::Byte> {};
template<> struct DataTypeOf<uint8_t> : std::integral_constant<DataType, DataType::UnsignedByte> {};
template <> struct DataTypeOf< int16_t> : std::integral_constant<DataType, DataType::Short> {};
template <> struct DataTypeOf<uint16_t> : std::integral_constant<DataType, DataType::UnsignedShort> {};
template <> struct DataTypeOf< int32_t> : std::integral_constant<DataType, DataType::Integer> {};
template <> struct DataTypeOf<uint32_t> : std::integral_constant<DataType, DataType::UnsignedInteger> {};
template <> struct DataTypeOf<float>    : std::integral_constant<DataType, DataType::Float> {};

class AttributeBinding {
public:
    DataType attributeType;
    uint8_t attributeSize;
    uint32_t attributeOffset;

    BufferID vertexBuffer;
    uint32_t vertexSize;
    uint32_t vertexOffset;

    friend bool operator==(const AttributeBinding &lhs, const AttributeBinding &rhs) {
        return std::tie(lhs.attributeType, lhs.attributeSize, lhs.attributeOffset, lhs.vertexBuffer, lhs.vertexSize, lhs.vertexOffset)
            == std::tie(rhs.attributeType, rhs.attributeSize, rhs.attributeOffset, rhs.vertexBuffer, rhs.vertexSize, rhs.vertexOffset);
    }
};

using AttributeBindingArray = std::vector<optional<AttributeBinding>>;

    /*
        Create a binding for this attribute.  The `attributeSize` parameter may be used to
        override the number of components available in the buffer for each vertex.  Thus,
        a buffer with only one float for each vertex can be bound to a `vec2` attribute
    */
template <class AttributeType, class Vertex>
AttributeBinding attributeBinding(const VertexBuffer<Vertex> &buffer, std::size_t attributeIndex, std::size_t attributeSize = AttributeType::Dimensions) {
    static_assert(std::is_standard_layout<Vertex>::value, "vertex type must use standard layout");
    assert(attributeSize >= 1);
    assert(attributeSize <= 4);
    assert(Vertex::attributeOffsets[attributeIndex] <= std::numeric_limits<uint32_t>::max());
    static_assert(sizeof(Vertex) <= std::numeric_limits<uint32_t>::max(), "vertex too large");
    return AttributeBinding {
        DataTypeOf<typename AttributeType::ValueType>::value,
        static_cast<uint8_t>(attributeSize),
        static_cast<uint32_t>(Vertex::attributeOffsets[attributeIndex]),
        buffer.buffer,
        static_cast<uint32_t>(sizeof(Vertex)),
        0
    };
}

optional<AttributeBinding> offsetAttributeBinding(optional<AttributeBinding> &binding, std::size_t vertexOffset);

class Context;
void bindAttributeLocation(Context &, ProgramID, AttributeLocation, const char *name);
std::set<std::string> getActiveAttributes(ProgramID);

template <class>
class Attributes;

template <class... As>
class Attributes<TypeList<As...>> final {
public:
    using Types = TypeList<As...>;
    using Locations = IndexedTuple<TypeList<As...>, TypeList<ExpandToType<As, optional<AttributeLocation>>...>>;
    using Bindings = IndexedTuple<TypeList<As...>, TypeList<ExpandToType<As, optional<AttributeBinding>>...>>;
    using NamedLocations = std::vector<std::pair<const std::string, AttributeLocation>>;

    static Locations bingLocations(Context &context, const ProgramID &id) {
        std::set<std::string> activeAttributes = getActiveAttributes(id);

        AttributeLocation location = 0;
        auto maybeAddLocation = [&](const char *name) ->optional<AttributeLocation> {
            if(activeAttributes.count(name)) {
                bindAttributeLocation(context, id, location, name);
                return location++;
            } else {
                return {};
            }
        };

        return Locations {maybeAddLocation(As::name)...};
    }

    template <class Program>
    static Locations loadNamedLocations(const Program& program) {
        return Locations{ program.attributeLocation(As::name())... };
    }

    static NamedLocations getNamedLocations(const Locations &locations) {
        NamedLocations result;

        auto maybeAddLocation = [&](const char *name, const optional<AttributeLocation> &location) {
            if(*location) {
                result.emplace_back(name, *location);
            }
        };

        util::ignore({(maybeAddLocation(As::name, locations.template get<As>()), 0)...});

        return result;
    }
    static Bindings bindings(const VertexBuffer<gfx::Vertex<Types>>& buffer) {
        return Bindings { attributeBinding<typename As::Type>(buffer, TypeIndex<As, As...>::value)... };
    }

    static Bindings offsetBindings(const Bindings& bindings, std::size_t vertexOffset) {
        return Bindings { offsetAttributeBinding(bindings.template get<As>(), vertexOffset)... };
    }

    static AttributeBindingArray toBindingArray(const Locations& locations, const Bindings& bindings) {
        AttributeBindingArray result;
        result.resize(sizeof...(As));

        auto maybeAddBinding = [&] (const optional<AttributeLocation>& location,
                                    const optional<AttributeBinding>& binding) {
            if (location) {
                result.at(*location) = binding;
            }
        };

        util::ignore({ (maybeAddBinding(locations.template get<As>(), bindings.template get<As>()), 0)... });

        return result;
    }

    static uint32_t activeBindingCount(const Bindings& bindings) {
        uint32_t result = 0;
        util::ignore({((result += bool(bindings.template get<As>())),0)...});
        return result;
    }

};
} // namespace gl
} // namespace mbgl
