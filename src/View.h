#ifndef PIPER_VIEW_H
#define PIPER_VIEW_H

#include <QGraphicsView>
#include <QByteArray>


namespace piper
{
    class CreatorPopup;

    class View : public QGraphicsView
    {
    public:
        View(QWidget* parent = nullptr);
        virtual ~View() = default;

        // Center the view on the items.
        void goHome();

    protected:
        void wheelEvent(QWheelEvent* event) override;
        void keyPressEvent(QKeyEvent * event) override;

        void mouseMoveEvent(QMouseEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;

    private:
        void copy();
        void paste();
        void undo();
        void redo();

        CreatorPopup* creator_;
        bool pan_{false};
        int panStartX_{};
        int panStartY_{};

        static QByteArray copy_;
    };
}

#endif
