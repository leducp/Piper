#ifndef PIPER_NODE_H
#define PIPER_NODE_H

#include "Scene.h"
#include "Attribute.h"
#include "Types.h"

namespace piper
{
    class NodeName : public QGraphicsTextItem
    {
    public:
        NodeName(QGraphicsItem* parent);
        virtual ~NodeName() = default;

        void adjustPosition();

    protected:
        void keyPressEvent(QKeyEvent* e) override;
    };

    class Node : public QGraphicsItem
    {
        friend Link* connect(QString const& from, QString const& out, QString const& to, QString const& in);
        friend QDataStream& operator<<(QDataStream& out, Node const& node);
        friend QDataStream& operator>>(QDataStream& in,  Node& node);

    public:
        Node (QString const& type = "", QString const& name = "", QString const& stage = "");
        virtual ~Node();

        // highlight attribute that are compatible with dataType
        void highlight(Attribute* emitter);
        void unhighlight();

        QString& stage()                { return stage_; } // TODO const it (currently required for stage edition)
        QString name() const;
        QString const& nodeType() const { return type_;  }

        void setMode(Mode mode);
        void setName(QString const& name);
        void setBackgroundColor(QColor const& color)
        {
            background_brush_.setColor(color);
            update();
        }
        void updateWidth();

        // Create attributes of this item.
        void createAttributes(QVector<AttributeInfo> const& attributesInfo);

        QVector<Attribute*> const& attributes() const { return attributes_; }

        QVector<Attribute*>& attributes() { return attributes_; }

    protected:
        QRectF boundingRect() const override;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;


    private:
        void createStyle();

        QRectF bounding_rect_;

        NodeName* name_;
        QString type_;
        QString stage_;
        Mode mode_;

        qint32 width_;
        qint32 height_;

        QBrush background_brush_;
        QPen pen_;
        QPen pen_selected_;

        QBrush attribute_brush_;
        QBrush attribute_alt_brush_;

        QPen type_pen_;
        QBrush type_brush_;
        QFont type_font_;
        QRectF type_rect_;

        QVector<Attribute*> attributes_;
    };

    Link* connect(QString const& from, QString const& out, QString const& to, QString const& in);

    QDataStream& operator<<(QDataStream& out, Node const& node);
    QDataStream& operator>>(QDataStream& in,  Node& node);
}

#endif

