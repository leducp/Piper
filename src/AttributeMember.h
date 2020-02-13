#ifndef NODE_ATTRIBUTE_MEMBER_H
#define NODE_ATTRIBUTE_MEMBER_H

#include "Attribute.h"

namespace piper
{
    class MemberForm : public QGraphicsTextItem
    {
    public:
        MemberForm(QGraphicsItem* parent);
        virtual ~MemberForm() = default;
        
    private:
        
    };

    class AttributeMember : public Attribute
    {
    public:
        AttributeMember(QGraphicsItem* parent, QString const& name, QString const& dataType, QRect const& boundingRect);
        
    protected:
        void paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*) override;    
        
        MemberForm* form_;
    };
}

#endif
