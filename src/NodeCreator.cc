#include "NodeCreator.h"

#include <QDebug>

namespace piper
{
    NodeCreator& NodeCreator::instance()
    {
        static NodeCreator creator_;
        return creator_;
    }

    void NodeCreator::addItem(Item const& item)
    {
        auto it = available_items_.find(item.type);
        if (it != available_items_.end())
        {
            qDebug() << "Can't add the item. Type" << item.type << "already exists.";
            return;
        }
        auto newItem = available_items_.insert(item.type, item);
        if (newItem->from == "")     { newItem->from = "unknown";     }
        if (newItem->category == "") { newItem->category = "unknown"; }
    }

    Node* NodeCreator::createItem(QString const& type, QString const& name, QString const& stage, const QPointF& pos)
    {
        auto it = available_items_.find(type);
        if (it == available_items_.end())
        {
            qDebug() << "Can't create the item" << name << ". Type" << type << "is unknown";
            return nullptr;
        }

        Node* node = new Node(type, name, stage);
        node->setPos(pos);
        node->createAttributes(it->attributes);
        node->setToolTip(it->help);
        return node;
    }
}
