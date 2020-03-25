#ifndef PIPER_NODE_H
#define PIPER_NODE_H

#include "Attribute.h"

namespace piper
{
    // Config
    QColor const default_background {80, 80, 80, 255};
    
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
        virtual ~Node() = default;
        
        // highlight attribute that are compatible with dataType
        void highlight(Attribute* emitter);
        void unhighlight();
        
        QString& stage()                 { return stage_; } //TODO const ?
        QString const& name() const      { return name_;  }
        QString const& nodeType()  const { return type_;  }
        
        void setName(QString const& name);
        void setBackgroundColor(QColor const& color)
        {
            background_brush_.setColor(color);
            update();
        }

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
    };

    Link* connect(QString const& from, QString const& out, QString const& to, QString const& in);
}

#endif 
  
