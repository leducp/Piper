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
        
    signals:
        void dataUpdated(int);
        void dataUpdated(double);
        void dataUpdated(QString const&);
        
    public slots:
        void onDataUpdated(QVariant const& data);
        
    protected:
        void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;   
        QRectF boundingRect() const override { return boundingRect_; }
        
    private:
        QVariant& data_; // reference on attribute's data
        QRectF boundingRect_;
        QBrush brush_;
    };

    ///\brief A Node attribute that can be edited by the user.
    class AttributeMember : public Attribute
    {
    public:
        AttributeMember(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect);
        virtual ~AttributeMember() = default;
        
        // Set the data by working closely with the MemberForm class.
        void setData(QVariant const& data) override;
        
    private:
        QWidget* createWidget();
        
        MemberForm* form_;
    };
}

#endif
