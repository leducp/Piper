#ifndef NODE_ATTRIBUTE_H
#define NODE_ATTRIBUTE_H

#include <QGraphicsItem>
#include <QPainter>

class NodePath;

class Attribute : public QGraphicsItem
{
public:
    Attribute(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect);
    virtual ~Attribute() = default;
    
    void setBrush(QBrush const& brush) { brush_ = brush; }
    void setFont(QFont const& font, QColor const& color)    
    { 
        font_ = font;   
        fontPen_.setStyle(Qt::SolidLine);
        fontPen_.setColor(color);
    }
    
    virtual QPointF connectorPos() const { return QPointF{}; }
    void connect(NodePath* path) { connections_.append(path); }
    void disconnect(NodePath* path) { connections_.removeAll(path); }
    void refresh();
    
    QString const& dataType() const { return dataType_; }
    
protected:
    QRectF boundingRect() const override { return boundingRect_; }
    void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
    
    QString name_;
    QString dataType_;
    
    QFont font_;
    QPen fontPen_;
    QBrush brush_;
    QPen pen_;
    
    QRectF boundingRect_;
    QRectF labelRect_;
    
    QList<NodePath*> connections_;
};

class AttributeOutput : public Attribute
{
public:
    AttributeOutput(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect);
    
    QPointF connectorPos() const override { return mapToScene(connectorPos_); }
    
protected:
    QRectF boundingRect() const override;
    void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    
    QPen penConnector_;
    QBrush brushConnector_;
    
    QRectF connectorRect_;
    QPointF connectorPos_;
    
    NodePath* newConnection_{nullptr};
};

class AttributeInput : public Attribute
{
public:
    AttributeInput(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect);
    
    bool accept(Attribute* attribute) const;
    QPointF connectorPos() const override { return mapToScene(connectorPos_); }
    
protected:
    void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
    int type() const override { return UserType + 1; }
    
    QPen penConnector_;
    QBrush brushConnector_;
    
    QPointF inputTriangle_[3];
    QPointF connectorPos_;
};


#endif
