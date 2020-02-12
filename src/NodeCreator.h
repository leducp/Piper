#ifndef NODE_CREATOR_H
#define NODE_CREATOR_H

#include <QLineEdit>

#include "NodeItem.h"


class NodeCreator 
{
public:
    NodeCreator() = default;
    virtual ~NodeCreator() = default;
    
    QList<QString> availableItems() const { return availableItems_.keys(); }
    void addItem(QString const& type, QList<AttributeInfo> const& attributes);
    NodeItem* createItem(QString const& type, QString const& name, QString const& stage, QPointF const& pos);
    
private:
    QHash<QString, QList<AttributeInfo>> availableItems_;
};


#endif
