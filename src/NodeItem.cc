#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QDebug>

#include "NodeItem.h"
#include "NodePath.h"

constexpr int attributeHeight = 30;
constexpr int baseHeight = 35;
constexpr int baseWidth  = 250;


NodeItem::NodeItem(QString const& name)
    : QGraphicsItem(nullptr)
    , width_(baseWidth)
    , height_(baseHeight)
    , name_(name)
    , attributes_{}
{  
    // Configure item behavior.
    //setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsFocusable);
    
    createStyle();
}


void NodeItem::addAttribute(AttributeInfo const& info)
{
    constexpr QRect boundingRect{0, 0, baseWidth-2, attributeHeight};
    
    NodeAttribute* attr;
    switch (info.type)
    {
        case AttributeInfo::Type::input:
        {
            attr = new NodeAttributeInput(this, info.name, info.dataType, boundingRect);
            break;
        }
        case AttributeInfo::Type::output:
        {
            attr = new NodeAttributeOutput(this, info.name, info.dataType, boundingRect);
            break;
        }
        case AttributeInfo::Type::member:
        {
            attr = new NodeAttributeMember(this, info.name, info.dataType, boundingRect);
            break;
        }
    }
    attr->setPos(1, 17 + attributeHeight * attributes_.size());
    if (attributes_.size() % 2)
    {
        attr->setBackgroundBrush(attrBrush_);
    }
    else
    {
        attr->setBackgroundBrush(attrAltBrush_);
    }
    height_ += attributeHeight;
    attributes_.append(attr);
}


void NodeItem::createStyle()
{
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
    attrBrush_.setColor({60, 60, 60, 255});
    attrAltBrush_.setStyle(Qt::SolidPattern);
    attrAltBrush_.setColor({70, 70, 70, 255});
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


void NodeItem::keyPressEvent(QKeyEvent* event)
{
    constexpr qreal moveFactor = 5;
    if ((event->key() == Qt::Key::Key_Up) and (event->modifiers() == Qt::NoModifier))
    {
        moveBy(0, -moveFactor);
    }
    if ((event->key() == Qt::Key::Key_Down) and (event->modifiers() == Qt::NoModifier))
    {
        moveBy(0, moveFactor);
    }
    if ((event->key() == Qt::Key::Key_Left) and (event->modifiers() == Qt::NoModifier))
    {
        moveBy(-moveFactor, 0);
    }
    if ((event->key() == Qt::Key::Key_Right) and (event->modifiers() == Qt::NoModifier))
    {
        moveBy(moveFactor, 0);
    }
    
    //QGraphicsItem::keyPressEvent(event);
}


NodePath* connect(NodeItem& from, QString const& out, NodeItem& to, QString const& in)
{
    NodeAttribute* attrOut{nullptr};
    for (auto& attr : from.attributes_)
    {
        if (attr->name() == out)
        {
            attrOut = attr;
            break;
        }
    }
    
    NodeAttribute* attrIn{nullptr};
    for (auto& attr : to.attributes_)
    {
        if (attr->name() == in)
        {
            attrIn = attr;
            break;
        }
    }
    
    if (attrIn == nullptr) 
    {
        qDebug() << "Can't find attribute" << in << "(in) in the node" << to.name();
        std::abort();
    }

    if (attrOut == nullptr)
    {
        qDebug() << "Can't find attribute" << out << "(out) in the node" << from.name();
        std::abort();
    }
    
    if (not attrIn->accept(attrOut))
    {
        qDebug() << "Can't connect attribute" << from.name() << "to attribute" << to.name();
        std::abort();
    }
    
    NodePath* path= new NodePath;
    path->connectFrom(attrOut);
    path->connectTo(attrIn);
    return path;
}
