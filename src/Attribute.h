#ifndef PIPER_ATTRIBUTE_H
#define PIPER_ATTRIBUTE_H

#include <QGraphicsItem>
#include <QPainter>

namespace piper
{
    class Link;

    enum DisplayMode
    {
        minimize,
        normal,
        highlight
    };


    class Attribute : public QGraphicsItem
    {
    public:
        Attribute (QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect);
        virtual ~Attribute();
        
        QString const& name() const { return name_; }
        virtual bool accept(Attribute*) const { return false; }
        void setBackgroundBrush(QBrush const& brush) { 
        background_brush_ = brush; }
        
        virtual QPointF connectorPos() const { return QPointF{}; }
        void connect(Link* path)    { connections_.append(path);    }
        void disconnect(Link* path) { connections_.removeAll(path); }
        void refresh();
        
        // Highlight compatible attributes and geyed out other.
        void highlight();
        
        // Revert back the highlight state.
        void unhighlight(); 
        
        QString const& dataType() const    { return data_type_; }
        QVariant const& data() const       { return data_; }
        virtual void setData(QVariant const& data) { data_ = data; } 
        
        void setMode(DisplayMode mode)  { mode_ = mode; }
        
        // Enable the use of qgraphicsitem_cast with this item.
        enum { Type = UserType + 1 };
        int type() const override { return Type; }
        
    protected:
        QRectF boundingRect() const override { return bounding_rect_; }
        void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
        
        void applyFontStyle(QPainter* painter, DisplayMode mode);
        void applyStyle(QPainter* painter, DisplayMode mode);
        
        QString name_;
        QString data_type_;
        QVariant data_;
        DisplayMode mode_{DisplayMode::normal};
        
        QBrush background_brush_;
        
        QFont minimize_font_;
        QPen minimize_font_pen_;
        QBrush minimize_brush_;
        QPen minimize_pen_;
        
        QFont normal_font_;
        QPen normal_font_pen_;
        QBrush normal_brush_;
        QPen normal_pen_;
        
        QFont highlight_font_;
        QPen highlight_font_pen_;
        QBrush highlight_brush_;
        QPen highlight_pen_;
        
        QRectF bounding_rect_;
        QRectF label_rect_;
        
        QList<Link*> connections_;
    };


    class AttributeOutput : public Attribute
    {
    public:
        AttributeOutput(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect);
        virtual ~AttributeOutput() = default;
        
        QPointF connectorPos() const override { return mapToScene(connectorPos_); }
        
    protected:
        QRectF boundingRect() const override;
        void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
        
        QRectF connectorRect_;
        QPointF connectorPos_;
        
        Link* new_connection_{nullptr};
    };


    class AttributeInput : public Attribute
    {
    public:
        AttributeInput(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect);
        virtual ~AttributeInput() = default;
        
        bool accept( Attribute* attribute) const override;
        QPointF connectorPos() const override { return mapToScene(connectorPos_); }
        
    protected:
        void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
        
        QPointF inputTriangle_[3];
        QPointF connectorPos_;
    };
}

#endif
