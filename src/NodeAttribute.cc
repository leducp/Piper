#include "NodeAttribute.h"


void Attribute::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
{
    painter->setBrush(brush_);
    painter->setPen(Qt::NoPen);
    painter->drawRect(boundingRect_);
    
    // Attribute label.
    
    QRect labelRect(
        boundingRect_.left() + 20, boundingRect_.top(),
        boundingRect_.width() - 2 * 10, boundingRect_.height());
    
    painter->setPen(fontPen_);
    painter->setFont(font_);
    painter->drawText(labelRect, Qt::AlignVCenter, name_);
}


QRectF AttributeOutput::boundingRect() const
{
    return boundingRect_.united(connectorRect_);
}


AttributeOutput::AttributeOutput(QGraphicsItem* parent, QString const& name, QRect const& boundingRect)
    : Attribute(parent, name, boundingRect)
{
    brushConnector_.setStyle(Qt::SolidPattern);
    brushConnector_.setColor({255, 155, 0, 255});
    penConnector_.setStyle(Qt::SolidLine);
    penConnector_.setColor({0, 0, 0, 255});
    
    // Compute connector rectangle.
    qint32 length = boundingRect_.height() / 4;
    
    connectorRect_ = QRect(boundingRect_.right() - length, length,
                           length * 2, length * 2);
    
}


void AttributeOutput::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
{
    // Draw generic part (label and background).
    Attribute::paint(painter, nullptr, nullptr);
    

    painter->setBrush(brushConnector_);
    painter->setPen(penConnector_);
    painter->drawEllipse(connectorRect_);
}


AttributeInput::AttributeInput(QGraphicsItem* parent, QString const& name, QRect const& boundingRect)
    : Attribute(parent, name, boundingRect)
{
    brushConnector_.setStyle(Qt::SolidPattern);
    brushConnector_.setColor({255, 155, 0, 255});
    penConnector_.setStyle(Qt::SolidLine);
    penConnector_.setColor({0, 0, 0, 255});
}


void AttributeInput::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
{
    // Draw generic part (label and background).
    Attribute::paint(painter, nullptr, nullptr);
    
    // Draw connector.
    qint32 length = boundingRect_.height() / 4;
    QPointF points[3] = 
    {
        QPointF(0, length),
        QPointF(length * 2, boundingRect_.height() / 2),
        QPointF(0, length * 3)
    };
    
    painter->setBrush(brushConnector_);
    painter->setPen(penConnector_);
    painter->drawConvexPolygon(points, 3);
}
