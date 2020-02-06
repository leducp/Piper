#include <QStyleOptionGraphicsItem>
//#include <QWidget>

#include "NodeItem.h"
#include <QDebug>

constexpr int attributeHeight = 30;
constexpr int baseHeight = 35;
constexpr int baseWidth  = 200;


NodeItem::NodeItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , width_(baseWidth)
    , height_(baseHeight)
    , name_("TargetFrontal")
    , attributes_{}
{
    setPos(-50, -50);
  
    createStyle();
    
    Attribute* attr = new Attribute(this, "heya", {0, 0, baseWidth-2, attributeHeight});
    attr->setFont({"Noto", 10, QFont::Normal}, {220, 220, 220, 255});
    attr->setBrush(attrBrush_);
    attr->setPos(1, 17);
    attributes_.append(attr);
    height_ += attributeHeight;
    
    attr = new AttributeInput(this, "heya", {0, 0, baseWidth-2, attributeHeight});
    attr->setFont({"Noto", 10, QFont::Normal}, {220, 220, 220, 255});
    attr->setBrush(attrAltBrush_);
    attr->setPos(1, 17 + attributeHeight);
    attributes_.append(attr);
    height_ += attributeHeight;
    
    attr = new AttributeOutput(this, "heya", {0, 0, baseWidth-2, attributeHeight});
    attr->setFont({"Noto", 10, QFont::Normal}, {220, 220, 220, 255});
    attr->setBrush(attrBrush_);
    attr->setPos(1, 17 + attributeHeight*2);
    attributes_.append(attr);
    height_ += attributeHeight;
}

/*
void NodeItem::addAttribute(QString const& name, Attribute const& info)
{
    attributes_.insert(name, info);
    height_ += attributeHeight;
}
*/

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
    //attrBrush_.setColor({160, 160, 160, 255});
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
    
    // attributes_
    /*
    qint32 border = 2;
    int offset = 0;
    QHashIterator<QString, Attribute> it(attributes_);
    while (it.hasNext()) 
    {
        it.next();
        QRect attributeRect(
                border / 2, baseHeight - radius + offset,
                baseWidth - border, attributeHeight);
        
        // alterne brush to enhance visibility
        if ((offset / attributeHeight) % 2)
        {
            painter->setBrush(attrAltBrush_);
        }
        else
        {
            painter->setBrush(attrBrush_);
        }
        painter->setPen(attrPen_);
        painter->drawRect(attributeRect);
        offset += attributeHeight;

        
        // Attribute label.
        QRect labelRect(
            attributeRect.left() + radius, attributeRect.top(),
            attributeRect.width() - 2 * radius, attributeRect.height());
        
        painter->setPen(attrFontPen_);
        painter->setFont(attrFont_);
        painter->drawText(labelRect, Qt::AlignVCenter, it.key());
        
    }
    */
}
