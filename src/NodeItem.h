#ifndef NODE_ITEM_H
#define NODE_ITEM_H

#include "NodeAttribute.h"

class NodeItem : public QGraphicsItem
{
public:
    NodeItem(QGraphicsItem* parent = nullptr);
    virtual ~NodeItem() = default;
    
    //void addAttribute(QString const& name, Attribute const& info);
    
protected:
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
private:
    void createStyle();
    
    qint32 width_;
    qint32 height_;
    QString name_;

    QBrush brush_;
    QPen pen_;
    QPen penSel_;
    
    QFont textFont_;
    QPen textPen_;
    QRect textRect_;
    
    QFont attrFont_;
    QPen attrFontPen_;
    QBrush attrBrush_;
    QBrush attrAltBrush_;
    QPen attrPen_;
    
    QList<Attribute*> attributes_;
};

#endif 
  
