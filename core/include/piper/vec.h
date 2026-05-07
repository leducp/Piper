#ifndef PIPER_VEC_H
#define PIPER_VEC_H

#include <ostream>
#include <string>

namespace piper
{
    // Plain aggregate. Component-wise arithmetic so generic step
    // templates (Add<T>, Subtract<T>, ...) instantiate cleanly when
    // T is one of these. No multiply/divide -- vector multiplication
    // is ambiguous (Hadamard / dot / cross / scalar) and forcing one
    // here would make the wrong choice for most callers; compose via
    // split_vec / make_vec if you need it.
    template<typename T>
    struct Vec2
    {
        T x{};
        T y{};
    };

    template<typename T>
    struct Vec3
    {
        T x{};
        T y{};
        T z{};
    };

    template<typename T>
    constexpr Vec2<T> operator+(Vec2<T> const& a, Vec2<T> const& b)
    {
        return Vec2<T>{ a.x + b.x, a.y + b.y };
    }
    template<typename T>
    constexpr Vec2<T> operator-(Vec2<T> const& a, Vec2<T> const& b)
    {
        return Vec2<T>{ a.x - b.x, a.y - b.y };
    }
    template<typename T>
    constexpr bool operator==(Vec2<T> const& a, Vec2<T> const& b)
    {
        return a.x == b.x and a.y == b.y;
    }

    template<typename T>
    constexpr Vec3<T> operator+(Vec3<T> const& a, Vec3<T> const& b)
    {
        return Vec3<T>{ a.x + b.x, a.y + b.y, a.z + b.z };
    }
    template<typename T>
    constexpr Vec3<T> operator-(Vec3<T> const& a, Vec3<T> const& b)
    {
        return Vec3<T>{ a.x - b.x, a.y - b.y, a.z - b.z };
    }
    template<typename T>
    constexpr bool operator==(Vec3<T> const& a, Vec3<T> const& b)
    {
        return a.x == b.x and a.y == b.y and a.z == b.z;
    }

    // Member-string format: "x,y" / "x,y,z". Whitespace tolerated
    // around commas. Throws std::runtime_error on malformed input.
    Vec2<float> parse_vec2f(std::string const& s);
    Vec3<float> parse_vec3f(std::string const& s);
}

#endif
