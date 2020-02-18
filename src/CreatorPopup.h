#ifndef PIPER_CREATOR_POPUP_H
#define PIPER_CREATOR_POPUP_H

#include <QLineEdit>
#include <QCompleter>
#include <QStringListModel>

#include "View.h"

namespace piper
{
    class CreatorPopup : public QLineEdit
    {
        Q_OBJECT
        
    public:
        CreatorPopup(View* view);
        virtual ~CreatorPopup() = default;
        
        void popup();
        void popdown();
        
    public slots:
        void onReturnPressed();
        
    protected:
        void focusOutEvent(QFocusEvent*) override;

    private:
        QStringListModel* model_;
        View* view_;
    };
}

#endif

