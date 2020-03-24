#ifndef PIPER_TAB_H
#define PIPER_TAB_H

#include <QTabWidget>
#include <QPushButton>
#include <QLineEdit>

namespace piper
{
    class EditorTab : public QTabWidget
    {
        Q_OBJECT
    public:
        EditorTab(QWidget* parent = nullptr);
        virtual ~EditorTab() = default;
        
    private slots:
        void createNewEditorTab();
        void closeEditorTab(int32_t index);
        
    private:
    };
    
}

#endif
