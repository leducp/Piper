#ifndef NODE_H
#define NODE_H

#include <QMainWindow>
#include "NodeScene.h"
#include "ui_node.h"

class Node : public QMainWindow
{
    Q_OBJECT

public:
    explicit Node(QWidget *parent = nullptr);
    virtual ~Node() = default;

private:
    Ui::Node ui_;
    NodeScene* scene_;
};

#endif 
