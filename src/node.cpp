#include "node.h"
#include "ui_node.h"

Node::Node(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::Node)
{
    m_ui->setupUi(this);
}

Node::~Node() = default;
