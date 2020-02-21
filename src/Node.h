#ifndef PIPER_NODE_H
#define PIPER_NODE_H

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
        friend Link* connect(QString const& from, QString const& out, QString const& to, QString const& in);
        
    public:
        Node (QString const& type, QString const& name, QString const& stage);
        virtual ~Node();
        
        // Get all created items.
        static QList<Node*> const& items();
        static void resetStagesColor();
        static void updateStagesColor(QString const& stage, QColor const& color);
        
        // highlight attribute that are compatible with dataType
        void highlight(Attribute* emitter);
        void unhighlight();
        
        QString& stage()                 { return stage_; }
        QString const& name() const      { return name_;  }
        QString const& nodeType()  const { return type_;  }
        
        void setName(QString const& name);

        // Add an attribute to this item.
        void addAttribute(AttributeInfo const& info);
        
        QList<Attribute*> const& attributes() const { return attributes_; }

    protected:
        QRectF boundingRect() const override;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
        
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        
    private:
        void createStyle();
        
        QRectF bounding_rect_;
        
        QString name_;
        QString type_;
        QString stage_;
        
        qint32 width_;
        qint32 height_;

        QBrush background_brush_;
        QPen pen_;
        QPen pen_selected_;
        
        QFont text_font_;
        QPen text_pen_;
        QRect text_rect_;
        
        QBrush attribute_brush_;
        QBrush attribute_alt_brush_;
        
        QList<Attribute*> attributes_;
        
        static QList<Node*> items_; // required to manage node items without dynamic casting all the scene items.
    };

    Link* connect(QString const& from, QString const& out, QString const& to, QString const& in);
}

#endif 
  
