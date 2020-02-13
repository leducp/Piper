#ifndef NODE_ITEM_H
#define NODE_ITEM_H

#include "Attribute.h"

namespace piper
{
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

    
    class Node : public QGraphicsItem
    {
        friend Link* connect(Node& from, QString const& out, Node& to, QString const& in);
        
    public:
        Node (QString const& type, QString const& name, QString const& stage);
        virtual ~Node();
        
        // Get all created items.
        static QList<Node*> const& items();
        
        // highlight attribute that are compatible with dataType
        void highlight(Attribute* emitter);
        void unhighlight();
        
        QString const& name()      const { return name_; }
        QString const& nodeType()  const { return type_; }
        QString const& stage()     const { return stage_; }
        
        void setBackgroundColor(QColor const& color);

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

        QBrush bgBrush_;
        QPen pen_;
        QPen penSel_;
        
        QFont textFont_;
        QPen textPen_;
        QRect textRect_;
        
        QBrush attrBrush_;
        QBrush attrAltBrush_;
        
        QList<Attribute*> attributes_;
        
        static QList<Node*> items_; // required to manage node items without dynamic casting all the scene items.
    };

    Link* connect( Node& from, QString const& out, Node& to, QString const& in);
}

#endif 
  
