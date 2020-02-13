#ifndef NODE_ITEM_H
#define NODE_ITEM_H

#include "NodeAttribute.h"

struct AttributeInfo 
{
    QString name;
    QString dataType;
    enum class Type
    {
        input,
        output,
        member
    } type;
};

class NodeItem : public QGraphicsItem
{
    friend NodePath* connect(NodeItem& from, QString const& out, NodeItem& to, QString const& in);
    
public:
    NodeItem(QString const& type, QString const& name, QString const& stage);
    virtual ~NodeItem();
    
    // Get all created items.
    static QList<NodeItem*> const& items();
    
    // highlight attribute that are compatible with dataType
    void highlight(NodeAttribute* emitter);
    void unhighlight();
    
    
    QString const& name() const { return name_; }

    // Add an attribute to this item.
    void addAttribute(AttributeInfo const& info);

    
protected:
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    
private:
    void createStyle();
    
    QString name_;
    QString type_;
    QString stage_;
    
    qint32 width_;
    qint32 height_;

    QBrush brush_;
    QPen pen_;
    QPen penSel_;
    
    QFont textFont_;
    QPen textPen_;
    QRect textRect_;
    
    QBrush attrBrush_;
    QBrush attrAltBrush_;
    
    QList<NodeAttribute*> attributes_;
    
    static QList<NodeItem*> items_; // required to manage node items without dynamic casting all the scene items.
};

NodePath* connect(NodeItem& from, QString const& out, NodeItem& to, QString const& in);

#endif 
  
