#ifndef PIPER_ATTRIBUTE_MEMBER_H
#define PIPER_ATTRIBUTE_MEMBER_H

#include "Attribute.h"

#include <QGraphicsProxyWidget>


namespace piper
{
    /// \brief Handle the member form (draw the backround and own the dedicated QWidget)
    class MemberForm : public QGraphicsProxyWidget
    {
        Q_OBJECT

    public:
        MemberForm(QGraphicsItem* parent, QVariant& data, QRectF const& boundingRect, QBrush const& brush);
        virtual ~MemberForm() = default;

        QRectF boundingRect() const override { return bounding_rect_; }
        void updateFormWidth(qreal data);

    signals:
        void dataUpdated(int);
        void dataUpdated(double);
        void dataUpdated(QString const&);

    public slots:
        void onDataUpdated(QVariant const& data);

    protected:
        void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

    private:
        QVariant& data_; // reference on attribute's data
        QRectF bounding_rect_;
        QBrush brush_;
    };

    ///\brief A Node attribute that can be edited by the user.
    class AttributeMember : public Attribute
    {
    public:
        AttributeMember(QGraphicsItem* parent, AttributeInfo const& info, QRect const& boundingRect);
        virtual ~AttributeMember() = default;

        // Set the data by working closely with the MemberForm class.
        void setData(QVariant const& data) override;
        void updateRectSize(QRectF rectangle) override;
        void setFormBaseWidth(qreal width);
        qreal getFormBaseWidth() const override { return baseWidth_; };

    private:
        QWidget* createWidget();

        MemberForm* form_;
        qreal baseWidth_;
    };
}

#endif
