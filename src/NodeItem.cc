#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QDebug>

#include "NodeItem.h"

constexpr int attributeHeight = 30;
constexpr int baseHeight = 35;
constexpr int baseWidth  = 200;


NodeItem::NodeItem(QString const& name)
    : QGraphicsItem(nullptr)
    , width_(baseWidth)
    , height_(baseHeight)
    , name_(name)
    , attributes_{}
{  
    createStyle();
}


void NodeItem::addAttribute(AttributeInfo const& info)
{
    constexpr QRect boundingRect{0, 0, baseWidth-2, attributeHeight};
    
    Attribute* attr;
    switch (info.type)
    {
        case AttributeInfo::Type::input:
        {
            attr = new AttributeInput(this, info.name, info.dataType, boundingRect);
            break;
        }
        case AttributeInfo::Type::output:
        {
            attr = new AttributeOutput(this, info.name, info.dataType, boundingRect);
            break;
        }
        case AttributeInfo::Type::member:
        {
            attr = new Attribute(this, info.name, info.dataType, boundingRect);
            break;
        }
    }
    attr->setFont(attrFont_, {220, 220, 220, 255});
    attr->setPos(1, 17 + attributeHeight * attributes_.size());
    if (attributes_.size() % 2)
    {
        attr->setBrush(attrBrush_);
    }
    else
    {
        attr->setBrush(attrAltBrush_);
    }
    height_ += attributeHeight;
    attributes_.append(attr);
}


void NodeItem::createStyle()
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);

    //QPointF nodeCenter(width_ / 2.0, height_ / 2.0);
    qint32 border = 2;

    brush_.setStyle(Qt::SolidPattern);
    brush_.setColor({80, 80, 80, 255});

    pen_.setStyle(Qt::SolidLine);
    pen_.setWidth(border);
    pen_.setColor({50, 50, 50, 255});
    //pen_.setColor({250, 250, 250, 255});

    penSel_.setStyle(Qt::SolidLine);
    penSel_.setWidth(border);
    penSel_.setColor({170, 80, 80, 255});


    textPen_.setStyle(Qt::SolidLine);
    textPen_.setColor({255, 255, 255, 255});

    textFont_ = QFont("Noto", 12, QFont::Bold);
    QFontMetrics metrics(textFont_);
    qint32 text_width = metrics.boundingRect(name_).width() + 14;
    qint32 text_height = metrics.boundingRect(name_).height() + 14;
    qint32 margin = (text_width - width_) * 0.5;
    textRect_ = QRect(-margin, -text_height, text_width, text_height);
    
    attrBrush_.setStyle(Qt::SolidPattern);
    attrBrush_.setColor({60, 60, 60, 255});
    attrAltBrush_.setStyle(Qt::SolidPattern);
    attrAltBrush_.setColor({70, 70, 70, 255});
    
    attrFont_ = QFont("Noto", 10, QFont::Normal);
    attrFontPen_.setStyle(Qt::SolidLine);
    attrFontPen_.setColor({220, 220, 220, 255});
    
    attrPen_.setStyle(Qt::SolidLine);
    attrPen_.setColor({0,0,0,0});
}

QRectF NodeItem::boundingRect() const
{
    return QRect(0, 0, width_, height_).united(textRect_);
}


void NodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    
    // Base shape.
    painter->setBrush(brush_);
    
    if (isSelected())
    {
        painter->setPen(penSel_);
    }
    else
    {
        painter->setPen(pen_);
    }
    
    qint32 radius = 10;
    painter->drawRoundedRect(0, 0, width_, height_, radius, radius);
    
    // Label.
    painter->setPen(textPen_);
    painter->setFont(textFont_);
    painter->drawText(textRect_, Qt::AlignCenter, name_);
}


void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    // Force selected node on top layer
    for (auto& item : scene()->items())
    {
        if (item->zValue() > 1)
        {
            item->setZValue(1);
        }
    }
    setZValue(2);
    
    QGraphicsItem::mousePressEvent(event);
}



void NodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    for (auto& attr : attributes_)
    {
        attr->refresh(); // let the attribute refresh their data if required.
    }
    
    QGraphicsItem::mouseMoveEvent(event);
}
