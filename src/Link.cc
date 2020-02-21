#include "Link.h"
#include "Node.h"

#include <cmath>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QGraphicsRectItem>

#include <QDebug>

namespace piper
{
    Waypoint::Waypoint(Link* parent)
        : QGraphicsRectItem(parent)
        , link_{parent}
    {
        setFlag(QGraphicsItem::ItemIsSelectable);
        setFlag(QGraphicsItem::ItemIsMovable);
        setFlag(QGraphicsItem::ItemIsFocusable);
    }
    
    
    Waypoint::~Waypoint()
    {
        link_->waypoints_.removeAll(this);
    }
    
    
    void Waypoint::keyPressEvent(QKeyEvent* event)
    {   
        if (event->key() == Qt::Key_Delete)
        {
            delete this;
        }
    }
    
    
    void Waypoint::updateNorm(QPointF const& from)
    { 
        norm_ = sqrt(pow(pos().x() - from.x(), 2) + pow(pos().y() - from.y(), 2));
    }
    
    
    QList<Link*> Link::items_{};
    QList<Link*> const& Link::items()
    {
        return items_;
    }
    
    Link::Link()
    {
        setFlag(QGraphicsItem::ItemIsSelectable);
        setFlag(QGraphicsItem::ItemIsFocusable);
        
        brush_.setStyle(Qt::SolidPattern);
        brush_.setColor({255, 155, 0, 255});
        
        pen_.setStyle(Qt::SolidLine);
        pen_.setColor({255, 155, 0, 255});
        pen_.setWidth(2);
        
        selected_.setStyle(Qt::SolidLine);
        selected_.setColor({255, 180, 180, 255});
        selected_.setWidth(3);
        
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
    
    
    void Link::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
    {
        if (isSelected())
        {
            setPen(selected_);
        }
        else
        {
            setPen(pen_);
        }
        
        if (to_ != nullptr)
        {
            updatePath();
        }
        QGraphicsPathItem::paint(painter, option, widget);
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

    
    void Link::keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_Delete)
        {
            delete this;
        }
    }
    
    
    void Link::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {    
        setSelected(true);
        
        // disconnect from end.
        to_->disconnect(this);
        
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
            }
        }
        else
        {
            connectTo(to_); // reset connection
        }
    }
    
    
    void Link::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
    {
        //TODO need more works to be enable
        /*
        Waypoint* waypoint = new Waypoint(this);
        waypoint->setPos(event->pos());
        waypoint->setRect(-5, -5, 10, 10);
        waypoint->setPen(pen_);
        waypoint->setBrush(brush_);
        waypoints_ << waypoint;
        */
    }

    
    void Link::updatePath(QPointF const& start, QPointF const& end)
    {
        if (waypoints_.isEmpty())
        {
            qreal dx = (end.x() - start.x()) * 0.5;
            qreal dy = (end.y() - start.y());
            QPointF c1{start.x() + dx, start.y() + dy * 0};
            QPointF c2{start.x() + dx, start.y() + dy * 1};
            
            QPainterPath path;
            path.moveTo(start);
            path.cubicTo(c1, c2, end);
            
            setPath(path);
            return;
        }
        
        for (auto& w : waypoints_)
        {
            w->updateNorm(start);
        }
        std::sort(waypoints_.begin(), waypoints_.end(), [](Waypoint* left, Waypoint* right){ return *left < *right; });
        
        QList<QPointF> waypoints;
        waypoints << start;
        for (auto const& w : waypoints_)
        {
            waypoints << w->pos();
        }
        waypoints << end;
        drawSplines(waypoints, 0.4);
    }
    
    
    void Link::computeControlPoint(QPointF const& p0, QPointF const& p1, QPointF const& p2, double t,
                                   QPointF& ctrl1, QPointF& ctrl2)
    {
        using namespace std;
        
        double d01 = sqrt(pow(p1.x()-p0.x(), 2) + pow(p1.y() - p0.y(), 2));
        double d12 = sqrt(pow(p2.x()-p1.x(), 2) + pow(p2.y() - p1.y(), 2));
        
        double fa = t * d01 / (d01 + d12);   // scaling factor for triangle Ta
        double fb = t * d12 / (d01 + d12);   // ditto for Tb, simplifies to fb=t-fa
        
        double p1x = p1.x() - fa * (p2.x() - p0.x()); // x2-x0 is the width of triangle T
        double p1y = p1.y() - fa * (p2.y() - p0.y()); // y2-y0 is the height of T
        ctrl1.setX(p1x);
        ctrl1.setY(p1y);
        
        double p2x = p1.x() + fb * (p2.x() - p0.x());
        double p2y = p1.y() + fb * (p2.y() - p0.y());
        ctrl2.setX(p2x);
        ctrl2.setY(p2y);
    }
    
    
    void Link::drawSplines(QList<QPointF> const& waypoints, double t)
    {
        // Compute control points
        QList<QPointF> controlPoints;
        for (int i = 0; i < waypoints.size() - 2; i += 1)
        {
            QPointF c1, c2;
            computeControlPoint(waypoints.at(i), waypoints.at(i + 1), waypoints.at(i + 2), t,
                                c1, c2);
            controlPoints << c1 << c2;
        }
        auto nextWaypoint = waypoints.cbegin();
        auto ctrl = controlPoints.cbegin();
        
        //  Prepare path -> first spline is a quadratic bezier curve
        QPainterPath path;
        path.moveTo(*(nextWaypoint++));        
        path.quadTo(*(ctrl++), *(nextWaypoint++));
        
        // draw others
        for (int i = 2; i < waypoints.size() - 1; i += 1)
        {
            path.cubicTo(*ctrl, *(ctrl+1), *(nextWaypoint++));
            ctrl += 2;
        }
        
        // finalize: last one is a quadratic bezier (like the first one)
        path.quadTo(*ctrl, *nextWaypoint);
        setPath(path);
    }
}
