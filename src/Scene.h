#ifndef PIPER_SCENE_H
#define PIPER_SCENE_H

#include <QGraphicsScene>

namespace piper
{
    class Link;
    class Node;
    
    class Scene : public QGraphicsScene
    {
        Q_OBJECT

    public:
        Scene(QObject *parent = nullptr);
        virtual ~Scene();
        
        void resetStagesColor();
        void updateStagesColor(QString const& stage, QColor const& color);
        
        void addNode(Node* node);
        void removeNode(Node* node);
        QList<Node*> const& nodes() const { return nodes_; }
        
        void addLink(Link* link);
        void removeLink(Link* link);
        QList<Link*> const& links() const { return links_; }
        void connect(QString const& from, QString const& out, QString const& to, QString const& in);
        
    protected:
        void drawBackground(QPainter *painter, const QRectF &rect) override;
        void keyReleaseEvent(QKeyEvent *keyEvent) override;
        
    private:
        QList<Node*> nodes_;
        QList<Link*> links_;
    };
}

#endif 
 
