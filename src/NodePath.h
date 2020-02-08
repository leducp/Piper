#ifndef NODE_PATH_H
#define NODE_PATH_H

#include "NodeAttribute.h"

#include <QGraphicsPathItem>

class NodePath : public QGraphicsPathItem
{
public:
    NodePath();
    virtual ~NodePath();
    
    void connectFrom(Attribute* from);    
    void connectTo(Attribute* to);
    
    void updatePath();
    void updatePath(QPointF const& end);
    
    Attribute const* from() const { return from_; }
    Attribute const* to() const   { return to_;   }
    
private:
    void updatePath(QPointF const& start, QPointF const& end);
    
    QPen pen_;
    QBrush brush_;
    
    Attribute* from_{nullptr};
    Attribute* to_{nullptr};
};

#endif
