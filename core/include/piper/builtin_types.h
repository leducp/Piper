#ifndef PIPER_BUILTIN_TYPES_H
#define PIPER_BUILTIN_TYPES_H

#include <stdint.h>

#include "piper/vec.h"

namespace piper
{
    template<typename... Ts>
    struct TypeList
    {
    };

    // Canonical pin tag for a C++ type: what lands in
    // AttributeSpec::data_type, in the saved JSON, and inside the
    // "<...>" of a Step type name. Declare one for a custom T with
    // PIPER_DECLARE_DATA_TYPE_TAG; data_type_string<T>() with an
    // undeclared T is a compile error rather than a silent mismatch
    // between the node registry and the step registry.
    template<typename T>
    struct DataTypeTag;

    template<typename T>
    constexpr char const* data_type_string()
    {
        return DataTypeTag<T>::name;
    }

    // The scalar set every builtin node family is instantiated for.
    // core (NodeType metadata) and engine (Step factories) both fold
    // over this list, so adding a type is one edit that cannot leave
    // the two registries disagreeing.
    using BuiltinScalars = TypeList<float,   double,
                                    int8_t,  int16_t,  int32_t,  int64_t,
                                    uint8_t, uint16_t, uint32_t, uint64_t>;

    // Types carrying the vector node families (constant/add/subtract).
    using BuiltinVectors = TypeList<Vec2<float>, Vec3<float>>;
}

// Use at global scope.
#define PIPER_DECLARE_DATA_TYPE_TAG(T, TAG)      \
    template<>                                   \
    struct piper::DataTypeTag<T>                 \
    {                                            \
        static constexpr char const* name = TAG; \
    }

PIPER_DECLARE_DATA_TYPE_TAG(float,    "float");
PIPER_DECLARE_DATA_TYPE_TAG(double,   "double");
PIPER_DECLARE_DATA_TYPE_TAG(int8_t,   "int8_t");
PIPER_DECLARE_DATA_TYPE_TAG(int16_t,  "int16_t");
PIPER_DECLARE_DATA_TYPE_TAG(int32_t,  "int32_t");
PIPER_DECLARE_DATA_TYPE_TAG(int64_t,  "int64_t");
PIPER_DECLARE_DATA_TYPE_TAG(uint8_t,  "uint8_t");
PIPER_DECLARE_DATA_TYPE_TAG(uint16_t, "uint16_t");
PIPER_DECLARE_DATA_TYPE_TAG(uint32_t, "uint32_t");
PIPER_DECLARE_DATA_TYPE_TAG(uint64_t, "uint64_t");

PIPER_DECLARE_DATA_TYPE_TAG(piper::Vec2<float>, "vec2<float>");
PIPER_DECLARE_DATA_TYPE_TAG(piper::Vec3<float>, "vec3<float>");

#endif
