#ifndef PIPER_SCENE_H
#define PIPER_SCENE_H

#include <QGraphicsScene>

namespace piper
{
    class Scene : public QGraphicsScene
    {
        Q_OBJECT

    public:
        Scene(QObject *parent = nullptr);
        virtual ~Scene() = default;
        
    protected:
        void drawBackground(QPainter *painter, const QRectF &rect) override;
        void keyReleaseEvent(QKeyEvent *keyEvent) override;
    };
}

#endif 
 
