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

    private:
        CreatorPopup* creator_;
    };
}

#endif
