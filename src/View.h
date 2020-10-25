#ifndef PIPER_VIEW_H
#define PIPER_VIEW_H

#include <QGraphicsView>


namespace piper
{
    class CreatorPopup;

    class View : public QGraphicsView
    {
    public:
        View(QWidget* parent = nullptr);
        virtual ~View() = default;

    protected:
        void wheelEvent(QWheelEvent* event) override;
        void keyPressEvent(QKeyEvent * event) override;
        
        void mouseMoveEvent(QMouseEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;

    private:
        CreatorPopup* creator_;
        bool pan_{false};
        int panStartX_{};
        int panStartY_{};
    };
}

#endif
