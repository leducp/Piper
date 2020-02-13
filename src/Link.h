#ifndef NODE_PATH_H
#define NODE_PATH_H

#include "Attribute.h"

#include <QGraphicsPathItem>

namespace piper
{
    class Link : public QGraphicsPathItem
    {
    public:
        Link();
        virtual ~Link();
        
        void connectFrom(Attribute* from);    
        void connectTo(Attribute* to);
        
        void updatePath();
        void updatePath(QPointF const& end);
        
        Attribute const* from() const { return from_; }
        Attribute const* to() const   { return to_;   }
        
    private:
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
        
        void updatePath(QPointF const& start, QPointF const& end);
        
        QPen pen_;
        QBrush brush_;
        
        Attribute* from_{nullptr};
        Attribute* to_{nullptr};
    };
}

#endif
