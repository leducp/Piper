#ifndef PIPER_TYPE_CHECK_H
#define PIPER_TYPE_CHECK_H

#include <string_view>

namespace piper
{
    class TypeCheck
    {
    public:
        virtual ~TypeCheck() = default;

        virtual bool compatible(std::string_view a, std::string_view b) const
        {
            return a == b;
        }
    };
}

#endif
