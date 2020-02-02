#include <QStyleOptionGraphicsItem>
//#include <QWidget>

#include "NodeItem.h"
#include <QDebug>

NodeItem::NodeItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , width_(200)
    , height_(25)
    , name_("TargetFrontal")
{
    setPos(-50, -50);
    addAttribute("yolo", {{}, "TestType", true, false });
    addAttribute("yala", {{17}, "", false, false });
    
    createStyle();
}


void NodeItem::addAttribute(QString const& name, AttributeInfo const& info)
{
    attributes_[name] = info;
    height_ += 30; // attribute height
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
    attrFont_ = QFont("Noto", 10, QFont::Normal);
    attrPen_.setStyle(Qt::SolidLine);

    //attrBrushAlt = QtGui.QBrush()
    //attrBrushAlt.setStyle(QtCore.Qt.SolidPattern)
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
    
        
}
