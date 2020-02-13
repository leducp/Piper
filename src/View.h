#ifndef NODE_VIEW_H
#define NODE_VIEW_H

#include <QGraphicsView>

namespace piper
{
    class View : public QGraphicsView
    {
    public:
        View(QWidget* parent = nullptr);
        virtual ~View() = default;
        
    protected:
        void wheelEvent(QWheelEvent* event) override;
        void keyPressEvent(QKeyEvent * event) override;
    };
}

#endif
