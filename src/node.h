#ifndef NODE_H
#define NODE_H

#include <QMainWindow>
#include <QScopedPointer>

namespace Ui {
class Node;
}

class Node : public QMainWindow
{
    Q_OBJECT

public:
    explicit Node(QWidget *parent = nullptr);
    ~Node() override;

private:
    QScopedPointer<Ui::Node> m_ui;
};

#endif // NODE_H
