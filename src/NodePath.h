#ifndef NODE_PATH_H
#define NODE_PATH_H

#include "NodeAttribute.h"

#include <QGraphicsPathItem>

class NodePath : public QGraphicsPathItem
{
public:
    NodePath();
    virtual ~NodePath();
    
    void connectFrom(NodeAttribute* from);    
    void connectTo(NodeAttribute* to);
    
    void updatePath();
    void updatePath(QPointF const& end);
    
    NodeAttribute const* from() const { return from_; }
    NodeAttribute const* to() const   { return to_;   }
    
    enum { Type = UserType + node::type::Path };
    int type() const override
    {
        // Enable the use of qgraphicsitem_cast with this item.
        return Type;
    }
    
private:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    
    void updatePath(QPointF const& start, QPointF const& end);
    
    QPen pen_;
    QBrush brush_;
    
    NodeAttribute* from_{nullptr};
    NodeAttribute* to_{nullptr};
};

#endif
