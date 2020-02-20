#ifndef PIPER_NODE_CREATOR_H
#define PIPER_NODE_CREATOR_H

#include "Node.h"

namespace piper
{
    class NodeCreator 
    {
    public:
        static NodeCreator& instance();
        
        QList<QString> availableItems() const { return available_items_.keys(); }
        void addItem(QString const& type, QList<AttributeInfo> const& attributes);
        Node* createItem(QString const& type, QString const& name, QString const& stage, QPointF const& pos);
        
    private:
        NodeCreator() = default;
        virtual ~NodeCreator() = default;
        
        QHash<QString, QList<AttributeInfo>> available_items_;
    };
}

#endif
