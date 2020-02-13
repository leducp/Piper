#include "AttributeMember.h"

namespace piper
{
    MemberForm::MemberForm(QGraphicsItem* parent)
        : QGraphicsTextItem(parent)
    {
    }



    NodeAttributeMember::NodeAttributeMember(QGraphicsItem* parent, const QString& name, const QString& dataType, const QRect& boundingRect)
        : Attribute (parent, name, dataType, boundingRect)
    {
        form_ = new MemberForm(this);
        form_->setTextInteractionFlags(Qt::TextEditable);
        form_->setPos(labelRect_.right() + 10, labelRect_.top() + 2);

        form_->setPlainText("lorem ipsum");
        form_->setFont(normalFont_);
        
        minimizeBrush_.setColor({150, 150, 150, 255});
    }


    void NodeAttributeMember::paint(QPainter* painter, QStyleOptionGraphicsItem const*, QWidget*)
    {
        // Draw generic part (label and background).
        Attribute::paint(painter, nullptr, nullptr);
        
        painter->setPen(Qt::NoPen);
        painter->setBrush(minimizeBrush_);
        painter->drawRoundedRect(labelRect_.right(), labelRect_.top() + 5, 
                                boundingRect_.width() * 2 / 3 - 20, boundingRect_.height() - 10, 
                                10, 10);
    }
}
