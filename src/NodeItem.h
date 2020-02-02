#ifndef NODE_ITEM_H
#define NODE_ITEM_H

#include <QGraphicsItem>
#include <QPainter>
#include <QHash>

struct AttributeInfo
{
    QVariant data;
    
    QString type;
    bool plug;
    bool socket;
};


class NodeItem : public QGraphicsItem
{
    //Q_OBJECT

public:
    NodeItem(QGraphicsItem* parent = nullptr);
    virtual ~NodeItem() = default;
    
    void addAttribute(QString const& name, AttributeInfo const& info);
    
protected:
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    
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
    QBrush attrBrush_;
    QPen attrPen_;
    
    QHash<QString, AttributeInfo> attributes_;
};

#endif 
  
