#include "EditorTab.h"

#include <QDebug>
#include <QTabBar> 
#include <QToolButton>

namespace piper
{
    EditorTab::EditorTab(QWidget* parent)
        : QTabWidget(parent)
    {
        // Create adder button
        QPushButton* tb = new QPushButton();
        tb->setFlat(true);
        tb->setIcon(QIcon(":/icon/add_tab.svg"));
        tb->setStyleSheet("* { icon-size: 24px 24px; }");
        setCornerWidget(tb, Qt::TopLeftCorner);
        
        tb->setMinimumSize(28, 28);
        tabBar()->setMinimumSize(28, 28);
    }

}
