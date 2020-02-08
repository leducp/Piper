#ifndef NODE_TYPE_H
#define NODE_TYPE_H

#include <QGraphicsItem>

namespace node
{
    enum type
    {
        Item = 1,
        Attribute = 2,
        AttributeInput = 3,
        AttributeOuput = 4,
        Path = 5
    };
}

#endif
