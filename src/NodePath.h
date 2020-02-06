#ifndef NODE_PATH_H
#define NODE_PATH_H

#include "NodeAttribute.h"

#include <QGraphicsPathItem>

class NodePath : public QGraphicsPathItem
{
public:
    NodePath(AttributeInput const& from, AttributeOutput const& to);
    
private:
    AttributeInput const& from_;
    AttributeOutput const& to_;
};

#endif
