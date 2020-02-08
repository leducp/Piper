#include "NodePath.h"

#include <QGraphicsScene>
#include <QDebug>

NodePath::NodePath()
{
    pen_.setStyle(Qt::SolidLine);
    pen_.setColor({255, 155, 0, 255});
    pen_.setWidth(2);
    setPen(pen_);
}


NodePath::~NodePath()
{
    if (from_ != nullptr)
    {
        from_->disconnect(this);
    }
    
    if (to_ != nullptr)
    {
        to_->disconnect(this);
    }
    
    scene()->removeItem(this);
}


void NodePath::connectFrom(Attribute* from)
{
     from_ = from; 
     from_->connect(this);
}


void NodePath::connectTo(Attribute* to)   
{ 
    to_ = to; 
    to_->connect(this); 
}


void NodePath::updatePath()
{
    updatePath(to_->connectorPos());
}


void NodePath::updatePath(QPointF const& end)
{
    updatePath(from_->connectorPos(), end);
    setZValue(-1); // force path to be under nodes
}


void NodePath::updatePath(QPointF const& start, QPointF const& end)
{
    qreal dx = (end.x() - start.x()) * 0.5;
    qreal dy = (end.y() - start.y());
    QPointF c1{start.x() + dx, start.y() + dy * 0};
    QPointF c2{start.x() + dx, start.y() + dy * 1};
    
    QPainterPath path;
    path.moveTo(start);
    path.cubicTo(c1, c2, end);
    
    setPath(path);
}
