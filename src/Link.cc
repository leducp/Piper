#include "Link.h"
#include "Node.h"

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>

namespace piper
{
    QList<Link*> Link::items_{};
    QList<Link*> const& Link::items()
    {
        return items_;
    }
    
    Link::Link()
    {
        pen_.setStyle(Qt::SolidLine);
        pen_.setColor({255, 155, 0, 255});
        pen_.setWidth(2);
        setPen(pen_);
        
        // add this to the items list;
        Link::items_.append(this);
    }

    Link::~Link()
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
        
        // Remove this from the items list.
        Link::items_.removeAll(this);
    }

    void Link::connectFrom(Attribute* from)
    {
        from_ = from; 
        from_->connect(this);
    }

    void Link::connectTo(Attribute* to)   
    { 
        to_ = to; 
        to_->connect(this); 
        updatePath();
    }

    void Link::updatePath()
    {
        updatePath(to_->connectorPos());
    }

    void Link::updatePath(QPointF const& end)
    {
        updatePath(from_->connectorPos(), end);
        setZValue(-1); // force path to be under nodes
    }

    void Link::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {    
        // disconnect from end.
        to_->disconnect(this);
        to_ = nullptr;
        
        // snap the path end to this point.
        updatePath(event->scenePos());
        
        // highlight available connections
        // Disable highlight
        for (auto& item : Node::items())
        {
            item->highlight(from_);
        }
    }

    void Link::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
    {
        // snap the path end to this point.
        updatePath(event->scenePos());
    }

    void Link::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
    {
        // Disable highlight
        for (auto& item : Node::items())
        {
            item->unhighlight();
        }
        
        // try to connect to the destinaton.
        AttributeInput* input = qgraphicsitem_cast<AttributeInput*>(scene()->itemAt(event->scenePos(), QTransform()));
        if (input != nullptr)
        {
            if (input->accept(from_))
            {
                connectTo(input);
                return;
            }
        }
        
        // We are not connected to an end: self destroy (there may be better way to do it though...)
        delete this;
    }

    void Link::updatePath(QPointF const& start, QPointF const& end)
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
}
