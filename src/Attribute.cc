#include "Attribute.h"
#include "Link.h"
#include "Node.h"
#include "Scene.h"
#include "ThemeManager.h"

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QMargins>
#include <QDebug>

#include <type_traits>

namespace piper
{
    QDataStream& operator<<(QDataStream& out, AttributeInfo const& info)
    {
        out << info.name << info.dataType << info.type;
        return out;
    }


    QDataStream& operator>>(QDataStream& in,  AttributeInfo& info)
    {
        int type;
        in >> info.name >> info.dataType >> type;
        info.type = static_cast<AttributeInfo::Type>(type);
        return in;
    }


    Attribute::Attribute (QGraphicsItem* parent, AttributeInfo const& info, QRect const& boundingRect)
        : QGraphicsItem(parent)
        , info_{info}
        , bounding_rect_{boundingRect}
        , background_rect_{bounding_rect_}
        , label_rect_{bounding_rect_.left() + 15, bounding_rect_.top(),
                      bounding_rect_.width() - 30, bounding_rect_.height()}
    {
        AttributeTheme theme = ThemeManager::instance().getAttributeTheme();
        DataTypeTheme typeTheme = ThemeManager::instance().getDataTypeTheme(dataType());

        minimize_pen_.setStyle(Qt::SolidLine);
        minimize_pen_.setWidth(theme.minimize.connector.border_width);
        minimize_pen_.setColor(theme.minimize.connector.border_color);
        minimize_font_ = theme.minimize.font;
        minimize_font_pen_.setStyle(Qt::SolidLine);
        minimize_font_pen_.setColor(theme.minimize.font_color);

        normal_pen_.setStyle(Qt::SolidLine);
        normal_pen_.setWidth(theme.normal.connector.border_width);
        normal_pen_.setColor(theme.normal.connector.border_color);
        normal_brush_.setStyle(Qt::SolidPattern);
        normal_brush_.setColor(typeTheme.enable);
        normal_font_ = theme.normal.font;
        normal_font_pen_.setStyle(Qt::SolidLine);
        normal_font_pen_.setColor(theme.normal.font_color);

        highlight_pen_.setStyle(Qt::SolidLine);
        highlight_pen_.setWidth(theme.highlight.connector.border_width);
        highlight_pen_.setColor(theme.highlight.connector.border_color);
        highlight_brush_.setStyle(Qt::SolidPattern);
        highlight_brush_.setColor(typeTheme.enable);
        highlight_font_ = theme.highlight.font;
        highlight_font_pen_.setStyle(Qt::SolidLine);
        highlight_font_pen_.setColor(theme.highlight.font_color);

        prepareGeometryChange();
    }


    Attribute::~Attribute()
    {
        // Disconnect related links.
        QVector<Link*> linksCopy = links_; //Create a copy: links_ as the list is altered while looping
        for (auto& link : linksCopy)
        {
            link->disconnect();
        }
    }


    void Attribute::setColor(QColor const& color)
    {
        normal_brush_.setColor(color);
        highlight_brush_.setColor(color);
        update();
    }


    void Attribute::connect(Link* link)
    {
        DataTypeTheme typeTheme = ThemeManager::instance().getDataTypeTheme(dataType());
        links_.append(link);
        link->setColor(typeTheme.enable);
    }



    void Attribute::refresh()
    {
        for (auto& link : links_)
        {
            link->updatePath();
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
                painter->setBrush(background_brush_);
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
        painter->drawRect(background_rect_);

        // NodeAttribute label.
        applyFontStyle(painter, mode_);
        painter->drawText(label_rect_, Qt::AlignVCenter, name());
    }



    AttributeOutput::AttributeOutput(QGraphicsItem* parent, AttributeInfo const& info, QRect const& boundingRect)
        : Attribute (parent, info, boundingRect)
    {

        // Compute connector rectangle.
        qreal const length = bounding_rect_.height() / 4.0;

        connector_rect_left_  = QRectF{ bounding_rect_.left() - length - 1, length,
                                      length * 2, length * 2 };

        connector_rect_right_ = QRectF{ bounding_rect_.right() - length + 1, length,
                                      length * 2, length * 2 };

         // Use data member to store connector position.
        setData(false);

        // Update bounding rect to include connector positions
        bounding_rect_ = bounding_rect_.united(connector_rect_left_);
        bounding_rect_ = bounding_rect_.united(connector_rect_right_);
        bounding_rect_ += QMargins(20, 0, 20, 0);
        prepareGeometryChange();
    }


    void AttributeOutput::setColor(QColor const& color)
    {
        Attribute::setColor(color);
        for (auto& link : links_)
        {
            link->setColor(color);
        }
    }



    void  AttributeOutput::setData(QVariant const& data)
    {
        if (data.canConvert(QMetaType::Bool))
        {
            data_ = data;
        }
        else
        {
            data_ = false;
        }

        if (data_.toBool())
        {
            connectorRect_ = &connector_rect_left_;
        }
        else
        {
            connectorRect_ = &connector_rect_right_;
        }
        // Compute connector center to position the path.
        connectorPos_ = { connectorRect_->x() + connectorRect_->width()  / 2.0,
                          connectorRect_->y() + connectorRect_->height() / 2.0 };
        update();
    }


    void AttributeOutput::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
    {
        // Draw generic part (label and background).
        Attribute::paint(painter, nullptr, nullptr);

        applyStyle(painter, mode_);
        painter->drawEllipse(*connectorRect_);
    }


    void AttributeOutput::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {
        Scene* pScene = static_cast<Scene*>(scene());

        if (connectorRect_->contains(event->pos()) and event->button() == Qt::LeftButton)
        {
            new_connection_ = new Link();
            new_connection_->connectFrom(this);
            new_connection_->setColor(normal_brush_.color());
            pScene->addLink(new_connection_);

            for (auto const& item : pScene->nodes())
            {
                item->highlight(this);
            }

            return;
        }

        if (event->button() == Qt::MiddleButton)
        {
            setData(not data_.toBool());
        }

        Attribute::mousePressEvent(event);
    }


    void AttributeOutput::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
    {
        if (new_connection_ == nullptr)
        {
            // Nothing to do
            Attribute::mouseMoveEvent(event);
            return;
        }

        new_connection_->updatePath(event->scenePos());
    }


    void AttributeOutput::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
    {
        Scene* pScene =  static_cast<Scene*>(scene());
        if ((new_connection_ == nullptr) or (event->button() != Qt::LeftButton))
        {
            // Nothing to do
            Attribute::mouseReleaseEvent(event);
            return;
        }

        // Disable highlight
        for (auto const& item : pScene->nodes())
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



    AttributeInput::AttributeInput (QGraphicsItem* parent, AttributeInfo const& info, QRect const& boundingRect)
        : Attribute (parent, info, boundingRect)
    {
        data_ = false; // Use data member to store connector position.

        // Compute input inputTriangle_
        qreal length = bounding_rect_.height() / 4.0;
        input_triangle_left_[0] = QPointF(bounding_rect_.left() - 1, length);
        input_triangle_left_[1] = QPointF(bounding_rect_.left() + length * 1.5, length * 2);
        input_triangle_left_[2] = QPointF(bounding_rect_.left() - 1, length * 3);

        input_triangle_right_[0] = QPointF(bounding_rect_.right() + 1, length);
        input_triangle_right_[1] = QPointF(bounding_rect_.right() - length * 1.5, length * 2);
        input_triangle_right_[2] = QPointF(bounding_rect_.right() + 1, length * 3);

        bounding_rect_ += QMargins(2, 0, 2, 0);
        prepareGeometryChange();

        setData(false);
    }


    void AttributeInput::setData(QVariant const& data)
    {
        if (data.canConvert(QMetaType::Bool))
        {
            data_ = data;
        }
        else
        {
            data_ = false;
        }

        if (data_.toBool())
        {
            input_triangle_ = input_triangle_right_;
        }
        else
        {
            input_triangle_ = input_triangle_left_;
        }

        // Compute connector center to position the path.
        qreal x = input_triangle_[0].x();;
        qreal y = input_triangle_[2].y() - input_triangle_[0].y();
        connectorPos_ = { x, y };
        update();
    }

    bool AttributeInput::accept(Attribute* attribute) const
    {
        if (attribute->dataType() != dataType())
        {
            // Incompatible type.
            return false;
        }

        if (attribute->parentItem() == parentItem())
        {
            // can't be connected to another attribute of the same item.
            return false;
        }

        for (auto& link : links_)
        {
            if (link->from() == attribute)
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
        painter->drawConvexPolygon(input_triangle_, 3);
    }


    void AttributeInput::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {
        if (event->button() == Qt::MiddleButton)
        {
            setData(not data_.toBool());
        }
        Attribute::mousePressEvent(event);
    }
}
