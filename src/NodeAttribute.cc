#include "NodeAttribute.h"
#include "NodePath.h"
#include "NodeItem.h"

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>

NodeAttribute::NodeAttribute(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect)
    : QGraphicsItem(parent)
    , name_{name}
    , dataType_{dataType}
    , boundingRect_{boundingRect}
    , labelRect_{boundingRect_.left() + 20, boundingRect_.top(), 
                 boundingRect_.width() - 2 * 10, boundingRect_.height()}
{ 
    minimizePen_.setStyle(Qt::SolidLine);
    minimizePen_.setColor({0, 0, 0, 255});
    minimizeBrush_.setStyle(Qt::SolidPattern);
    minimizeBrush_.setColor({80, 80, 80, 255});
    minimizeFont_ = QFont("Noto", 10, QFont::Light);
    minimizeFontPen_.setStyle(Qt::SolidLine);
    minimizeFontPen_.setColor({220, 220, 220, 255});
    
    normalPen_.setStyle(Qt::SolidLine);
    normalPen_.setColor({0, 0, 0, 255});
    normalBrush_.setStyle(Qt::SolidPattern);
    normalBrush_.setColor({255, 155, 0, 255});
    normalFont_ = QFont("Noto", 10, QFont::Normal);
    normalFontPen_.setStyle(Qt::SolidLine);
    normalFontPen_.setColor({220, 220, 220, 255});
    
    highlightPen_.setStyle(Qt::SolidLine);
    highlightPen_.setWidth(2);
    highlightPen_.setColor({250, 250, 250, 255});
    highlightBrush_.setStyle(Qt::SolidPattern);
    highlightBrush_.setColor({255, 155, 0, 255});
    highlightFont_ = QFont("Noto", 10, QFont::Medium);
    highlightFontPen_.setStyle(Qt::SolidLine);
    highlightFontPen_.setColor({220, 220, 220, 255});
}


NodeAttribute::~NodeAttribute()
{
    // We shall remove related connections (connection parent is the scene).
    for (auto& c : connections_)
    {
        delete c;
    }
}


void NodeAttribute::refresh()
{
    for (auto& c : connections_)
    {
        c->updatePath();
    }
}


void NodeAttribute::applyFontStyle(QPainter* painter, DisplayMode mode)
{
    switch (mode)
    {
        case highlight: 
        { 
            painter->setFont(highlightFont_);
            painter->setPen(highlightFontPen_);
            break;
        }
        case normal:
        {
            painter->setFont(normalFont_);
            painter->setPen(normalFontPen_);
            break;
        }
        case minimize:
        {
            painter->setFont(minimizeFont_);
            painter->setPen(minimizeFontPen_);
            break;
        }
    }    
}

void NodeAttribute::applyStyle(QPainter* painter, DisplayMode mode)
{
    switch (mode)
    {
        case highlight: 
        { 
            painter->setBrush(highlightBrush_);
            painter->setPen(highlightPen_);
            break;
        }
        case normal:
        {
            painter->setBrush(normalBrush_);
            painter->setPen(normalPen_);
            break;
        }
        case minimize:
        {
            painter->setBrush(minimizeBrush_);
            painter->setPen(minimizePen_);
            break;
        }
    }    
}


void NodeAttribute::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
{
    // NodeAttribute background.
    painter->setBrush(backgroundBrush_);
    painter->setPen(Qt::NoPen);
    painter->drawRect(boundingRect_);
    
    // NodeAttribute label.    
    applyFontStyle(painter, mode_);
    painter->drawText(labelRect_, Qt::AlignVCenter, name_);
}


QRectF NodeAttributeOutput::boundingRect() const
{
    return boundingRect_.united(connectorRect_);
}


NodeAttributeOutput::NodeAttributeOutput(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect)
    : NodeAttribute(parent, name, dataType, boundingRect)
{
    normalBrush_.setStyle(Qt::SolidPattern);
    normalBrush_.setColor({255, 155, 0, 255});
    
    // Compute connector rectangle.
    qint32 length = boundingRect_.height() / 4;
    
    connectorRect_ = QRect(boundingRect_.right() - length + 1, length,
                           length * 2, length * 2);
    
    // Compute connector center to position the path.
    connectorPos_ = { connectorRect_.x() + connectorRect_.width() / 2.0, 
                      connectorRect_.y() + connectorRect_.height() / 2.0 };
}


void NodeAttributeOutput::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
{
    // Draw generic part (label and background).
    NodeAttribute::paint(painter, nullptr, nullptr);
    
    applyStyle(painter, DisplayMode::normal);
    painter->drawEllipse(connectorRect_);
}


void NodeAttributeOutput::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (connectorRect_.contains(event->pos()) and event->button() == Qt::LeftButton)
    {
        newConnection_ = new NodePath;
        newConnection_->connectFrom(this);
        scene()->addItem(newConnection_);
        
        // Highlight compatible socket.
       QList<QGraphicsItem*> items = scene()->items();
       for (auto& item : items)
       {
            NodeAttribute* attr = qgraphicsitem_cast<NodeAttribute*>(item);
            if (attr != nullptr)
            {
                if (attr->dataType() == dataType_)
                {
                    attr->setMode(DisplayMode::highlight);
                }
                else
                {
                    attr->setMode(DisplayMode::minimize);
                }
            }
       }
       scene()->update(); // force redraw.
    
       return;
    }
    
    NodeAttribute::mousePressEvent(event);
}


void NodeAttributeOutput::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (newConnection_ == nullptr)
    {
        // Nothing to do
        NodeAttribute::mousePressEvent(event);
        return;
    }
    
    newConnection_->updatePath(event->scenePos());
}


void NodeAttributeOutput::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if ((newConnection_ == nullptr) or (event->button() != Qt::LeftButton))
    {
        // Nothing to do
        NodeAttribute::mousePressEvent(event);
        return;
    }
    
    // Disable highlight
    QList<QGraphicsItem*> items = scene()->items();
    for (auto& item : items)
    {
        NodeAttribute* attr = qgraphicsitem_cast<NodeAttribute*>(item);
        if (attr != nullptr)
        {
            attr->setMode(normal);
        }
    }
    scene()->update(); // force redraw.
    
    NodeAttributeInput* input = qgraphicsitem_cast<NodeAttributeInput*>(scene()->itemAt(event->scenePos(), QTransform()));
    if (input != nullptr)
    {
        if (input->accept(this))
        {
            newConnection_->connectTo(input);
            newConnection_->updatePath();
            newConnection_ = nullptr; // connection finished.
            return;
        }
    }
    
    // cleanup unfinalized connection.
    delete newConnection_;
    newConnection_ = nullptr;
}



NodeAttributeInput::NodeAttributeInput(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect)
    : NodeAttribute(parent, name, dataType, boundingRect)
{
    // Compute input inputTriangle_
    qreal length = boundingRect_.height() / 4.0;
    inputTriangle_[0] = QPointF(-1, length);
    inputTriangle_[1] = QPointF(length * 1.5, length * 2);
    inputTriangle_[2] = QPointF(-1, length * 3);
    
    connectorPos_ = { length * 1.5 / 3.0, length * 2 };
}


bool NodeAttributeInput::accept(NodeAttribute* attribute) const
{
    if (attribute->dataType() != dataType())
    {
        // Incompatible type.
        return false;
    }
    
    for (auto& c : connections_)
    {
        if (c->from() == attribute)
        {
            // We are already connected to this guy.
            return false;
        }
    }
    
    return true;
}


void NodeAttributeInput::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
{
    // Draw generic part (label and background).
    NodeAttribute::paint(painter, nullptr, nullptr);
    
    applyStyle(painter, mode_);
    painter->drawConvexPolygon(inputTriangle_, 3);
}

