#ifndef NODE_VIEW_H
#define NODE_VIEW_H

#include <QGraphicsView>

class NodeView : public QGraphicsView
{
public:
    NodeView(QWidget* parent = nullptr);
    virtual ~NodeView() = default;
    
protected:
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent * event) override;
};

#endif
