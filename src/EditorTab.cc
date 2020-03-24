#include "EditorTab.h"
#include "EditorWidget.h"

#include <QDebug>
#include <QTabBar> 
#include <QToolButton>
#include <QEvent>
#include <QFocusEvent>

namespace piper
{
    class TabHeaderEdit : public QLineEdit
    {
    public:
        TabHeaderEdit(QWidget* parent = nullptr) : QLineEdit(parent) { }
        virtual ~TabHeaderEdit() = default;
        
        void mousePressEvent(QMouseEvent *event) override
        {
            QLineEdit::mousePressEvent(event);
            QWidget::mousePressEvent(event); // let the parent do its work too !
        }
    };
    
    EditorTab::EditorTab(QWidget* parent)
        : QTabWidget(parent)
    {
        // Create adder button
        QPushButton* tb = new QPushButton();
        tb->setFlat(true);
        tb->setIcon(QIcon(":/icon/add_tab.svg"));
        tb->setStyleSheet("* { icon-size: 30px 30px; }");
        setCornerWidget(tb, Qt::TopLeftCorner);

        // Ensure that button will be always visible.
        setStyleSheet("QTabBar::tab { height: 40px; }");
        tb->setMinimumSize(40, 40);
        tabBar()->setMinimumSize(40, 40);
        
        // Connect signal to manage tab creation/rename/deletion
        QObject::connect(tb,   &QPushButton::clicked,            this, &EditorTab::createNewEditorTab);
        QObject::connect(this, &QTabWidget::tabCloseRequested,   this, &EditorTab::closeEditorTab);
    }
    
    
    void EditorTab::createNewEditorTab()
    {
        EditorWidget* editor = new EditorWidget();
        int32_t index = addTab(editor, "");
        
        QLineEdit* edit = new TabHeaderEdit();
        edit->setText("pipeline " + QString::number(index));
        edit->setFrame(false);
        edit->setStyleSheet("QLineEdit { background:transparent; }");
        tabBar()->setTabButton(index, QTabBar::LeftSide, edit);
    }

    
    void EditorTab::closeEditorTab(int32_t index)
    {
        delete widget(index); // Note: removeTab do not destroy the widget.
    }
}
