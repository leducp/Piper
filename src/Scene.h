#ifndef PIPER_SCENE_H
#define PIPER_SCENE_H

#include <QGraphicsScene>
#include "Node.h"

namespace piper
{
    class Link;
    
    class Scene : public QGraphicsScene
    {
        Q_OBJECT

    public:
        Scene(QObject *parent = nullptr);
        virtual ~Scene() = default;
        
        void resetStagesColor();
        void updateStagesColor(QString const& stage, QColor const& color);
        
        void addNode(Node* node);
        QList<Node*> const& nodes() const { return nodes_; }
        Link* connect(QString const& from, QString const& out, QString const& to, QString const& in);
        
    protected:
        void drawBackground(QPainter *painter, const QRectF &rect) override;
        void keyReleaseEvent(QKeyEvent *keyEvent) override;
        
    private:
        QList<Node*> nodes_;
    };
}

#endif 
 
