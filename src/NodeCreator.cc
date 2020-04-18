#include "NodeCreator.h"

#include <QDebug>

namespace piper
{
    NodeCreator& NodeCreator::instance()
    {
        static NodeCreator creator_;
        return creator_;
    }

    void NodeCreator::addItem(QString const& type, QVector<AttributeInfo> const& attributes)
    {
        QHash<QString, QVector<AttributeInfo>>::iterator it = available_items_.find(type);
        if (it != available_items_.end())
        {
            qDebug() << "Can't add the item. Type" << type << "already exists.";
            return;
        }
        available_items_.insert(type, attributes);
    }

    Node* NodeCreator::createItem(QString const& type, QString const& name, QString const& stage, const QPointF& pos)
    {
        QHash<QString, QVector<AttributeInfo>>::iterator it = available_items_.find(type);
        if (it == available_items_.end())
        {
            qDebug() << "Can't create the item" << name << ". Type" << type << "is unknown";
            return nullptr;
        }

        Node* node = new Node(type, name, stage);
        node->setPos(pos);
        node->createAttributes(*it);
        return node;
    }
}
