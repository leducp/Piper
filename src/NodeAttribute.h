#ifndef NODE_ATTRIBUTE_H
#define NODE_ATTRIBUTE_H

#include <QGraphicsItem>
#include <QPainter>

class Attribute : public QGraphicsItem
{
public:
    Attribute(QGraphicsItem* parent, QString const& name, QRect const& boundingRect)
        : QGraphicsItem(parent)
        , name_{name}
        , boundingRect_{boundingRect}
    { 
        pen_.setStyle(Qt::SolidLine);
        pen_.setColor({0, 0, 0, 0});
    }
    
    void setBrush(QBrush const& brush) { brush_ = brush; }
    void setFont(QFont const& font, QColor const& color)    
    { 
        font_ = font;   
        fontPen_.setStyle(Qt::SolidLine);
        fontPen_.setColor(color);
    }
    
protected:
    QRectF boundingRect() const override { return boundingRect_; }
    void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
    
    QString name_;
    
    QFont font_;
    QPen fontPen_;
    QBrush brush_;
    QPen pen_;
    
    QRect boundingRect_;
};

class AttributeOutput : public Attribute
{
public:
    AttributeOutput(QGraphicsItem* parent, QString const& name, QRect const& boundingRect);
    
    QPointF outputPos() const;
    
protected:
    QRectF boundingRect() const override;
    void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
    
    QPen penConnector_;
    QBrush brushConnector_;
    
    QRect connectorRect_;
};

class AttributeInput : public Attribute
{
public:
    AttributeInput(QGraphicsItem* parent, QString const& name, QRect const& boundingRect);
    
    QPointF inputPos() const;
    
protected:
    void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
    
    QPen penConnector_;
    QBrush brushConnector_;
};


#endif
