#include "CreatorPopup.h"
#include "NodeCreator.h"
#include "Scene.h"

#include <QAbstractItemView>
#include <QPointF>
#include <QEvent>
#include <QKeyEvent>

namespace piper
{
    CreatorPopup::CreatorPopup(View* view)
        : QLineEdit(view)
        , view_{view}
    {
        model_ = new QStringListModel();
        QCompleter* completer = new QCompleter(model_, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        completer->setMaxVisibleItems(20);
        completer->popup()->setStyleSheet(
                "background:transparent; "
                "border: 1px solid #ff9b00; "
                "color: #F2F2F2; "
                "selection-background-color: #E68A00; "
        );
        setCompleter(completer);
        setStyleSheet(
            "QLineEdit { "
                "background:transparent; "
                "border: 1px solid #ff9b00; "
                "color: #F2F2F2; "
            "}"
        );
        
        QObject::connect(this, &QLineEdit::returnPressed, this, &CreatorPopup::onReturnPressed);
        
        popdown();
    }
    
    
    void CreatorPopup::popup()
    {
        // Adjust size and populate content.
        QStringList types;
        QSize targetSize = size();
        for (auto const& item : NodeCreator::instance().availableItems())
        {
             QSize fontSize = fontMetrics().boundingRect(item.type).size();
             types << item.type;
             targetSize.setWidth(std::max(targetSize.width(), fontSize.width() + 30)); // +30px for margin
        }
        model_->setStringList(types);
        resize(targetSize);
        
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

    
    bool CreatorPopup::event(QEvent* event)
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Tab)
            {
                QModelIndex index = completer()->currentIndex();
                QString type = model_->itemData(index)[Qt::EditRole].toString();
                setText(type);
                return true;
            }
        }
        return QWidget::event(event);
    }
}
