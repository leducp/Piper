#include "CreatorPopup.h"
#include "NodeCreator.h"
#include "Scene.h"

#include <QPointF>
#include <QDebug>

namespace piper
{
    CreatorPopup::CreatorPopup(View* view)
        : QLineEdit(view)
        , view_{view}
    {
        model_ = new QStringListModel();
        QCompleter* completer = new QCompleter(model_, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        setCompleter(completer);
        
        QObject::connect(this, &QLineEdit::returnPressed, this, &CreatorPopup::onReturnPressed);
        
        popdown();
    }
    
    
    void CreatorPopup::popup()
    {
        model_->setStringList(NodeCreator::instance().availableItems());
        
        QPoint position = parentWidget()->mapFromGlobal(QCursor::pos());
        move(position);
        clear();
        show();
        setFocus();
        
        // display all solution at first glance.
        completer()->complete();
    }

    
    void CreatorPopup::popdown()
    {
        hide();
        clear();
        view_->setFocus();
    }
    
    
    void CreatorPopup::onReturnPressed()
    {
        QPointF scenePos = view_->mapToScene(pos());
        Scene* piperScene = static_cast<Scene*>(view_->scene());
        
        QString type = text();
        popdown();
        
        QString nextName = type + "_" + QString::number(piperScene->nodes().size());
        Node* node = NodeCreator::instance().createItem(type, nextName, "", scenePos);
        if (node != nullptr)
        {
            piperScene->addNode(node);
        }
    }

    
    void CreatorPopup::focusOutEvent(QFocusEvent*)
    {
        popdown();
    }

}
