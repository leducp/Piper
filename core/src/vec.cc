#include <stdexcept>
#include <string>

#include "piper/vec.h"

namespace piper
{
    namespace
    {
        float parse_float(std::string const& s, std::size_t& consumed)
        {
            return std::stof(s, &consumed);
        }

        // Consume comma + optional whitespace, advance pos.
        void eat_comma(std::string const& s, std::size_t& pos)
        {
            while (pos < s.size() and std::isspace(static_cast<unsigned char>(s[pos])))
            {
                ++pos;
            }
            if (pos >= s.size() or s[pos] != ',')
            {
                throw std::runtime_error("vec parse: expected ','");
            }
            ++pos;
        }
    }

    Vec2<float> parse_vec2f(std::string const& s)
    {
        std::size_t consumed = 0;
        float const x = parse_float(s, consumed);
        std::size_t pos = consumed;
        eat_comma(s, pos);
        std::string const tail = s.substr(pos);
        std::size_t consumed2 = 0;
        float const y = parse_float(tail, consumed2);
        return Vec2<float>{ x, y };
    }

    Vec3<float> parse_vec3f(std::string const& s)
    {
        std::size_t consumed = 0;
        float const x = parse_float(s, consumed);
        std::size_t pos = consumed;
        eat_comma(s, pos);
        std::string mid = s.substr(pos);
        std::size_t consumed2 = 0;
        float const y = parse_float(mid, consumed2);
        pos += consumed2;
        eat_comma(s, pos);
        std::string tail = s.substr(pos);
        std::size_t consumed3 = 0;
        float const z = parse_float(tail, consumed3);
        return Vec3<float>{ x, y, z };
    }
}
