#ifndef PIPER_ATTRIBUTE_H
#define PIPER_ATTRIBUTE_H

#include <QGraphicsItem>
#include <QPainter>

namespace piper
{
    class Link;

    struct AttributeInfo
    {
        QString name;
        QString dataType;
        enum Type
        {
            input  = 0,
            output = 1,
            member = 2
        } type;
    };

    QDataStream& operator<<(QDataStream& out, AttributeInfo const& info);
    QDataStream& operator>>(QDataStream& in,  AttributeInfo& info);

    enum DisplayMode
    {
        minimize,
        normal,
        highlight
    };


    class Attribute : public QGraphicsItem
    {
    public:
        Attribute (QGraphicsItem* parent, AttributeInfo const& info, QRect const& boundingRect);
        virtual ~Attribute();

        AttributeInfo const& info() const { return info_; }
        QString const& name() const     { return info_.name; }
        QString const& dataType() const { return info_.dataType; }
        bool isInput() const  { return (info_.type == AttributeInfo::Type::input);  }
        bool isOutput() const { return (info_.type == AttributeInfo::Type::output); }
        bool isMember() const { return (info_.type == AttributeInfo::Type::member); }

        void setBackgroundBrush(QBrush const& brush) { background_brush_ = brush; }
        virtual void setColor(QColor const& color);
        virtual void updateConnectorPosition(){}


        virtual QPointF connectorPos() const  { return QPointF{}; }
        virtual bool accept(Attribute*) const { return false; }
        void connect(Link* link);
        void disconnect(Link* link) { links_.removeAll(link); }
        void refresh();

        // Highlight compatible attributes and geyed out other.
        void highlight();

        // Revert back the highlight state.
        void unhighlight();

        QVariant const& data() const { return data_; }
        virtual void setData(QVariant const& data) { data_ = data; }

        void setMode(DisplayMode mode)  { mode_ = mode; }
        virtual void updateRectSize(QRectF rectangle);
        QRectF boundingRect() const override { return bounding_rect_; }
        QRectF labelRect() const { return label_rect_; }
        virtual qreal getFormBaseWidth() const { return 0; };


        // Enable the use of qgraphicsitem_cast with this item.
        enum { Type = UserType + 1 };
        int type() const override { return Type; }

    protected:
        void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;

        void applyFontStyle(QPainter* painter, DisplayMode mode);
        void applyStyle(QPainter* painter, DisplayMode mode);

        AttributeInfo info_;
        QVariant data_;
        DisplayMode mode_{DisplayMode::normal};

        QBrush background_brush_;

        QFont minimize_font_;
        QPen minimize_font_pen_;
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
        QRectF background_rect_;
        QRectF label_rect_;

        QVector<Link*> links_;
    };


    class AttributeOutput : public Attribute
    {
    public:
        AttributeOutput(QGraphicsItem* parent, AttributeInfo const& info, QRect const& boundingRect);
        virtual ~AttributeOutput() = default;

        void setColor(QColor const& color) override;
        void updateConnectorPosition() override;
        void setData(QVariant const& data) override;
        QPointF connectorPos() const override { return mapToScene(connectorPos_); }

    protected:
        void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

        QRectF connector_rect_left_;
        QRectF connector_rect_right_;
        QRectF* connectorRect_;
        QPointF connectorPos_;

        Link* new_connection_{nullptr};
    };


    class AttributeInput : public Attribute
    {
    public:
        AttributeInput(QGraphicsItem* parent, AttributeInfo const& info, QRect const& boundingRect);
        virtual ~AttributeInput() = default;

        void setData(QVariant const& data) override;
        void updateConnectorPosition() override;
        bool accept(Attribute* attribute) const override;
        QPointF connectorPos() const override { return mapToScene(connectorPos_); }

    protected:
        void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

        QPointF input_triangle_left_[3];
        QPointF input_triangle_right_[3];
        QPointF* input_triangle_;
        QPointF connectorPos_;
    };
}

#endif
