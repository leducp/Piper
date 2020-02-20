#include "Attribute.h"
#include "Link.h"
#include "Node.h"

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QDebug>

namespace piper
{
    Attribute::Attribute (QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect)
        : QGraphicsItem(parent)
        , name_{name}
        , data_type_{dataType}
        , bounding_rect_{boundingRect}
        , label_rect_{bounding_rect_.left() + 15, bounding_rect_.top(), 
                     bounding_rect_.width() - 30, bounding_rect_.height()}
    { 
        minimize_pen_.setStyle(Qt::SolidLine);
        minimize_pen_.setColor({0, 0, 0, 255});
        minimize_brush_.setStyle(Qt::SolidPattern);
        minimize_brush_.setColor({80, 80, 80, 255});
        minimize_font_ = QFont("Noto", 10, QFont::Light);
        minimize_font_pen_.setStyle(Qt::SolidLine);
        minimize_font_pen_.setColor({220, 220, 220, 255});
        
        normal_pen_.setStyle(Qt::SolidLine);
        normal_pen_.setColor({0, 0, 0, 255});
        normal_brush_.setStyle(Qt::SolidPattern);
        normal_brush_.setColor({255, 155, 0, 255});
        normal_font_ = QFont("Noto", 10, QFont::Normal);
        normal_font_pen_.setStyle(Qt::SolidLine);
        normal_font_pen_.setColor({220, 220, 220, 255});
        
        highlight_pen_.setStyle(Qt::SolidLine);
        highlight_pen_.setWidth(2);
        highlight_pen_.setColor({250, 250, 250, 255});
        highlight_brush_.setStyle(Qt::SolidPattern);
        highlight_brush_.setColor({255, 155, 0, 255});
        highlight_font_ = QFont("Noto", 10, QFont::Medium);
        highlight_font_pen_.setStyle(Qt::SolidLine);
        highlight_font_pen_.setColor({220, 220, 220, 255});
    }


    Attribute::~Attribute()
    {
        // We shall remove related connections (connection parent is the scene).
        for (auto& c : connections_)
        {
            delete c;
        }
    }


    void Attribute::refresh()
    {
        for (auto& c : connections_)
        {
            c->updatePath();
        }
    }


    void Attribute::applyFontStyle(QPainter* painter, DisplayMode mode)
    {
        switch (mode)
        {
            case DisplayMode::highlight: 
            { 
                painter->setFont(highlight_font_);
                painter->setPen(highlight_font_pen_);
                break;
            }
            case DisplayMode::normal:
            {
                painter->setFont(normal_font_);
                painter->setPen(normal_font_pen_);
                break;
            }
            case DisplayMode::minimize:
            {
                painter->setFont(minimize_font_);
                painter->setPen(minimize_font_pen_);
                break;
            }
        }    
    }

    void Attribute::applyStyle(QPainter* painter, DisplayMode mode)
    {
        switch (mode)
        {
            case DisplayMode::highlight: 
            { 
                painter->setBrush(highlight_brush_);
                painter->setPen(highlight_pen_);
                break;
            }
            case DisplayMode::normal:
            {
                painter->setBrush(normal_brush_);
                painter->setPen(normal_pen_);
                break;
            }
            case DisplayMode::minimize:
            {
                painter->setBrush(minimize_brush_);
                painter->setPen(minimize_pen_);
                break;
            }
        }    
    }


    void Attribute::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
    {
        // NodeAttribute background.
        painter->setBrush(background_brush_);
        painter->setPen(Qt::NoPen);
        painter->drawRect(bounding_rect_);
        
        // NodeAttribute label.    
        applyFontStyle(painter, mode_);
        painter->drawText(label_rect_, Qt::AlignVCenter, name_);
    }


    QRectF AttributeOutput::boundingRect() const
    {
        return bounding_rect_.united(connectorRect_);
    }


    AttributeOutput::AttributeOutput(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect)
        : Attribute (parent, name, dataType, boundingRect)
    {
        // Compute connector rectangle.
        qint32 length = bounding_rect_.height() / 4;
        
        connectorRect_ = QRect(bounding_rect_.right() - length + 1, length,
                            length * 2, length * 2);
        
        // Compute connector center to position the path.
        connectorPos_ = { connectorRect_.x() + connectorRect_.width() / 2.0, 
                        connectorRect_.y() + connectorRect_.height() / 2.0 };
    }


    void AttributeOutput::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
    {
        // Draw generic part (label and background).
        Attribute::paint(painter, nullptr, nullptr);
        
        applyStyle(painter, mode_);
        painter->drawEllipse(connectorRect_);
    }


    void AttributeOutput::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {
        if (connectorRect_.contains(event->pos()) and event->button() == Qt::LeftButton)
        {
            new_connection_ = new Link;
            new_connection_->connectFrom(this);
            scene()->addItem(new_connection_);
            
            for (auto& item : Node::items())
            {
                item->highlight(this);
            }
        
        return;
        }
        
        Attribute::mousePressEvent(event);
    }


    void AttributeOutput::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
    {
        if (new_connection_ == nullptr)
        {
            // Nothing to do
            Attribute::mousePressEvent(event);
            return;
        }
        
        new_connection_->updatePath(event->scenePos());
    }


    void AttributeOutput::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
    {
        if ((new_connection_ == nullptr) or (event->button() != Qt::LeftButton))
        {
            // Nothing to do
            Attribute::mousePressEvent(event);
            return;
        }
        
        // Disable highlight
        for (auto& item : Node::items())
        {
            item->unhighlight();
        }
        
        AttributeInput* input = qgraphicsitem_cast<AttributeInput*>(scene()->itemAt(event->scenePos(), QTransform()));
        if (input != nullptr)
        {
            if (input->accept(this))
            {
                new_connection_->connectTo(input);
                new_connection_ = nullptr; // connection finished.
                return;
            }
        }
        
        // cleanup unfinalized connection.
        delete new_connection_;
        new_connection_ = nullptr;
    }



    AttributeInput::AttributeInput (QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect)
        : Attribute (parent, name, dataType, boundingRect)
    {
        // Compute input inputTriangle_
        qreal length = bounding_rect_.height() / 4.0;
        inputTriangle_[0] = QPointF(-1, length);
        inputTriangle_[1] = QPointF(length * 1.5, length * 2);
        inputTriangle_[2] = QPointF(-1, length * 3);
        
        connectorPos_ = { length * 1.5 / 3.0, length * 2 };
    }


    bool AttributeInput::accept( Attribute* attribute) const
    {
        if (attribute->dataType() != dataType())
        {
            // Incompatible type.
            return false;
        }
        
        if (attribute->parentItem() == parentItem())
        {
            // can't be conencted to another attribute of the same item.
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
        
        applyStyle(painter, mode_);
        painter->drawConvexPolygon(inputTriangle_, 3);
    }
}
