#ifndef PIPER_NODE_CREATOR_H
#define PIPER_NODE_CREATOR_H

#include "Node.h"

namespace piper
{
    struct Item
    {
        QString type;                       // Item type - shall be unique!
        QString help;                       // Displayed text as a tooltip
        QString from;                       // The library that add the item
        QString category;                   // Item category to sort them in the interface
        QVector<AttributeInfo> attributes;  // Describe the behavior
    };
    
    class NodeCreator
    {
    public:
        static NodeCreator& instance();

        QList<Item> availableItems() const { return available_items_.values(); }
        void addItem(Item const& item);
        Node* createItem(QString const& type, QString const& name, QString const& stage, QPointF const& pos);

    private:
        NodeCreator() = default;
        virtual ~NodeCreator() = default;

        QHash<QString, Item> available_items_;
    };
}

#endif
