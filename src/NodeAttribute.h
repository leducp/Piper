#ifndef NODE_ATTRIBUTE_H
#define NODE_ATTRIBUTE_H

#include "Types.h"
#include <QPainter>

class NodePath;

enum DisplayMode
{
    minimize,
    normal,
    highlight
};

class NodeAttribute : public QGraphicsItem
{
public:
    NodeAttribute(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect);
    virtual ~NodeAttribute();
    
    void setBackgroundBrush(QBrush const& brush) { backgroundBrush_ = brush; }

    
    virtual QPointF connectorPos() const { return QPointF{}; }
    void connect(NodePath* path) { connections_.append(path); }
    void disconnect(NodePath* path) { connections_.removeAll(path); }
    void refresh();
    
    QString const& dataType() const { return dataType_; }
    
    void setMode(DisplayMode mode)  { mode_ = mode; }
    
    
    enum { Type = UserType + node::type::Attribute };
    int type() const override
    {
        // Enable the use of qgraphicsitem_cast with this item.
        return Type;
    }
    
protected:
    QRectF boundingRect() const override { return boundingRect_; }
    void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
    
    void applyFontStyle(QPainter* painter, DisplayMode mode);
    void applyStyle(QPainter* painter, DisplayMode mode);
    
    QString name_;
    QString dataType_;
    DisplayMode mode_{DisplayMode::normal};
    
    QBrush backgroundBrush_;
    
    QFont minimizeFont_;
    QPen minimizeFontPen_;
    QBrush minimizeBrush_;
    QPen minimizePen_;
    
    QFont normalFont_;
    QPen normalFontPen_;
    QBrush normalBrush_;
    QPen normalPen_;
    
    QFont highlightFont_;
    QPen highlightFontPen_;
    QBrush highlightBrush_;
    QPen highlightPen_;
    
    QRectF boundingRect_;
    QRectF labelRect_;
    
    QList<NodePath*> connections_;
};

class NodeAttributeOutput : public NodeAttribute
{
public:
    NodeAttributeOutput(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect);
    
    QPointF connectorPos() const override { return mapToScene(connectorPos_); }
    
protected:
    QRectF boundingRect() const override;
    void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    
    QRectF connectorRect_;
    QPointF connectorPos_;
    
    NodePath* newConnection_{nullptr};
};

class NodeAttributeInput : public NodeAttribute
{
public:
    NodeAttributeInput(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect);
    
    bool accept(NodeAttribute* attribute) const;
    QPointF connectorPos() const override { return mapToScene(connectorPos_); }
    
protected:
    void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
    
    QPointF inputTriangle_[3];
    QPointF connectorPos_;
};


#endif
