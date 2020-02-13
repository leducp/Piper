#ifndef NODE_SCENE_H
#define NODE_SCENE_H

#include <QGraphicsScene>

namespace piper
{
    class Scene : public QGraphicsScene
    {
        Q_OBJECT

    public:
        Scene(QObject *parent = nullptr);
        virtual ~Scene() = default;
        
    public slots:
        void changeStageColor(QString const& stage, QColor const& color);
    
        
    protected:
        void drawBackground(QPainter *painter, const QRectF &rect) override;
        void keyReleaseEvent(QKeyEvent *keyEvent) override;
    };
}

#endif 
 
