#ifndef PIPER_TAB_H
#define PIPER_TAB_H

#include <QTabWidget>
#include <QPushButton>

namespace piper
{
    class EditorTab : public QTabWidget
    {
    public:
        EditorTab(QWidget* parent = nullptr);
        virtual ~EditorTab() = default;
        
    private:
        
        
    };
    
}

#endif
