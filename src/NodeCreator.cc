#include "NodeCreator.h"

#include <QDebug>

namespace piper
{
    void NodeCreator::addItem(QString const& type, QList<AttributeInfo> const& attributes)
    {
        QHash<QString, QList<AttributeInfo>>::iterator it = availableItems_.find(type);
        if (it != availableItems_.end())
        {
            qDebug() << "Can't add the item. Type" << type << "already exists.";
            return;
        }
        availableItems_.insert(type, attributes);
    }

    Node* NodeCreator::createItem(QString const& type, QString const& name, QString const& stage, const QPointF& pos)
    {
        QHash<QString, QList<AttributeInfo>>::iterator it = availableItems_.find(type);
        if (it == availableItems_.end())
        {
            qDebug() << "Can't create the item" << name << ". Type" << type << "is unknown";
            return nullptr;
        }
        
        Node* node = new Node (type, name, stage);
        node->setPos(pos);
        
        for (auto const& attr : *it)
        {
            node->addAttribute(attr);
        }
        
        return node;
    }
}
