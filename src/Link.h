#ifndef PIPER_LINK_H
#define PIPER_LINK_H

#include "Attribute.h"

#include <QGraphicsPathItem>

namespace piper
{
    class Link : public QGraphicsPathItem
    {
    public:
        Link();
        virtual ~Link();
        
        // Get all created items.
        static QList<Link*> const& items();
        
        void connectFrom(Attribute* from);    
        void connectTo(Attribute* to);
        
        void updatePath();
        void updatePath(QPointF const& end);
        
        Attribute const* from() const { return from_; }
        Attribute const* to() const   { return to_;   }
        
    private:
        
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
        
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
        
        void updatePath(QPointF const& start, QPointF const& end);
        
        // Compute bezier control point to 'glue' properly two bezier curves
        void computeControlPoint(QPointF const& p0, QPointF const& p1, QPointF const& p2, double t,
                                 QPointF& ctrl1, QPointF& ctrl2);
        void drawSplines(QList<QPointF> const& waypoints, double t);
        
        QPen pen_;
        QPen selected_;
        QBrush brush_;
        
        Attribute* from_{nullptr};
        Attribute* to_{nullptr};
    };
    
    
    QDataStream& operator<<(QDataStream& out, Link const& link);
    QDataStream& operator>>(QDataStream& in,  Link& link);
}

#endif
