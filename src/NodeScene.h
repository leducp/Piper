#ifndef NODE_SCENE_H
#define NODE_SCENE_H

#include <QGraphicsScene>

class NodeScene : public QGraphicsScene
{
    Q_OBJECT

public:
    NodeScene(QObject *parent = nullptr);
    virtual ~NodeScene() = default;
    
protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;

};

#endif 
 
