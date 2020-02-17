#include "CreatorPopup.h"
#include "NodeCreator.h"

#include <QPointF>
#include <QDebug>

namespace piper
{
    CreatorPopup::CreatorPopup(View* view)
        : QLineEdit(nullptr)
        , view_{view}
    {
        model_ = new QStringListModel();
        completer_ = new QCompleter(model_, this);
        completer_->setCaseSensitivity(Qt::CaseInsensitive);
        setCompleter(completer_);
        QObject::connect(completer_, QOverload<const QString &>::of(&QCompleter::activated), this, &CreatorPopup::onCompleterActivated);
        QObject::connect(this, &QLineEdit::returnPressed, this, &CreatorPopup::onReturnPressed);
    }

    
    
    void CreatorPopup::popup()
    {
        model_->setStringList(NodeCreator::instance().availableItems());
        
        QPoint position = parentWidget()->mapFromGlobal(QCursor::pos());
        move(position);
        clear();
        show();
        setFocus();
    }

    
    void CreatorPopup::popdown()
    {
        hide();
        clear();
        view_->setFocus();
    }

    
    void CreatorPopup::onCompleterActivated(QString const& text)
    {
        
    }
    
    
    void CreatorPopup::onReturnPressed()
    {
        QPoint viewPos = view_->mapFromGlobal(QCursor::pos());
        QPointF scenePos = view_->mapToScene(viewPos);
        
        QString type = text();
        qDebug() << viewPos;
        
        popdown();
        Node* node = NodeCreator::instance().createItem(type, "NoName", "", scenePos);
        if (node != nullptr)
        {
            view_->scene()->addItem(node);
        }
    }

    
    void CreatorPopup::focusOutEvent(QFocusEvent*)
    {
        popdown();
    }

}
