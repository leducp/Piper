#include "NodeAttribute.h"
#include "NodePath.h"

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>

Attribute::Attribute(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect)
    : QGraphicsItem(parent)
    , name_{name}
    , dataType_{dataType}
    , boundingRect_{boundingRect}
    , labelRect_{boundingRect_.left() + 20, boundingRect_.top(), 
                 boundingRect_.width() - 2 * 10, boundingRect_.height()}
{ 
    pen_.setStyle(Qt::SolidLine);
    pen_.setColor({0, 0, 0, 0});
}


void Attribute::refresh()
{
    for (auto& c : connections_)
    {
        c->updatePath();
    }
}


void Attribute::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
{
    // Attribute background.
    painter->setBrush(brush_);
    painter->setPen(Qt::NoPen);
    painter->drawRect(boundingRect_);
    
    // Attribute label.    
    painter->setPen(fontPen_);
    painter->setFont(font_);
    painter->drawText(labelRect_, Qt::AlignVCenter, name_);
}


QRectF AttributeOutput::boundingRect() const
{
    return boundingRect_.united(connectorRect_);
}


AttributeOutput::AttributeOutput(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect)
    : Attribute(parent, name, dataType, boundingRect)
{
    brushConnector_.setStyle(Qt::SolidPattern);
    brushConnector_.setColor({255, 155, 0, 255});
    penConnector_.setStyle(Qt::SolidLine);
    penConnector_.setColor({0, 0, 0, 255});
    
    // Compute connector rectangle.
    qint32 length = boundingRect_.height() / 4;
    
    connectorRect_ = QRect(boundingRect_.right() - length + 1, length,
                           length * 2, length * 2);
    
    // Compute connector center to position the path.
    connectorPos_ = { connectorRect_.x() + connectorRect_.width() / 2.0, 
                      connectorRect_.y() + connectorRect_.height() / 2.0 };
}


void AttributeOutput::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
{
    // Draw generic part (label and background).
    Attribute::paint(painter, nullptr, nullptr);
    
    painter->setBrush(brushConnector_);
    painter->setPen(penConnector_);
    painter->drawEllipse(connectorRect_);
}


void AttributeOutput::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (connectorRect_.contains(event->pos()) and event->button() == Qt::LeftButton)
    {
        newConnection_ = new NodePath;
        newConnection_->connectFrom(this);
        scene()->addItem(newConnection_);
        return;
    }
    
    Attribute::mousePressEvent(event);
}


void AttributeOutput::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (newConnection_ == nullptr)
    {
        // Nothing to do
        Attribute::mousePressEvent(event);
        return;
    }
    
    QRectF boundingBox{event->scenePos(), QSizeF{50, 50}};
    
    newConnection_->updatePath(event->scenePos());
}


void AttributeOutput::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if ((newConnection_ == nullptr) or (event->button() != Qt::LeftButton))
    {
        // Nothing to do
        Attribute::mousePressEvent(event);
        return;
    }
    
    AttributeInput* input = qgraphicsitem_cast<AttributeInput*>(scene()->itemAt(event->scenePos(), QTransform()));
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



AttributeInput::AttributeInput(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect)
    : Attribute(parent, name, dataType, boundingRect)
{
    brushConnector_.setStyle(Qt::SolidPattern);
    brushConnector_.setColor({255, 155, 0, 255});
    penConnector_.setStyle(Qt::SolidLine);
    penConnector_.setColor({0, 0, 0, 255});
    
    // Compute input inputTriangle_
    qreal length = boundingRect_.height() / 4.0;
    inputTriangle_[0] = QPointF(-1, length);
    inputTriangle_[1] = QPointF(length * 1.5, length * 2);
    inputTriangle_[2] = QPointF(-1, length * 3);
    
    connectorPos_ = { length * 1.5 / 3.0, length * 2 };
}


bool AttributeInput::accept(Attribute* attribute) const
{
    if (attribute->dataType() != dataType())
    {
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


void AttributeInput::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
{
    // Draw generic part (label and background).
    Attribute::paint(painter, nullptr, nullptr);
    
    // Draw connector.    
    painter->setBrush(brushConnector_);
    painter->setPen(penConnector_);
    painter->drawConvexPolygon(inputTriangle_, 3);
}

