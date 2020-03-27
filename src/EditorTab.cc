#include "EditorTab.h"
#include "EditorWidget.h"

#include <QDebug>
#include <QTabBar> 
#include <QMouseEvent>
#include <QPushButton>

namespace piper
{

    TabHeaderEdit::TabHeaderEdit(QString const& text) 
        : QLineEdit(text) 
    { }
        
        
    void TabHeaderEdit::mousePressEvent(QMouseEvent *event) 
    {
        QLineEdit::mousePressEvent(event);
        QWidget::mousePressEvent(event); // let the parent do its work too !
    }


    TabNameValidator::TabNameValidator(QObject* parent) 
        : QValidator(parent) 
    { }
    
    
    QValidator::State TabNameValidator::validate(QString& input, int& pos) const
    {
        for (auto const& c : input)
        {
            // Refuse non plain ascii name.
            if (c.unicode() > 127)
            {
                return QValidator::Invalid;
            }
        }
        
        return QValidator::Acceptable;
    }
    
    
    EditorTab::EditorTab(QWidget* parent)
        : QTabWidget(parent)
        , tabNameValidator_(new TabNameValidator(this))
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
    
    
    QString EditorTab::name(int32_t index) const
    {
       TabHeaderEdit* edit = static_cast<TabHeaderEdit*>(tabBar()->tabButton(index, QTabBar::LeftSide));
       return edit->text();
    }
    
    
    void EditorTab::setName(int32_t index, QString const& name)
    {
        TabHeaderEdit* edit = static_cast<TabHeaderEdit*>(tabBar()->tabButton(index, QTabBar::LeftSide));
        edit->setText(name);
    }
    
    
    EditorWidget* EditorTab::createNewEditorTab()
    {
        EditorWidget* editor = new EditorWidget();
        int32_t index = addTab(editor, "");
        
        QString defaultText = "unamed pipeline";
        tabNameValidator_->fixup(defaultText);
        
        TabHeaderEdit* edit = new TabHeaderEdit(defaultText);
        edit->setFrame(false);
        edit->setStyleSheet("QLineEdit { background:transparent; }");
        edit->setValidator(tabNameValidator_);
        tabBar()->setTabButton(index, QTabBar::LeftSide, edit);
        QObject::connect(edit, &QLineEdit::editingFinished, this, &EditorTab::tabNameEdited);
        
        return editor;
    }

    
    void EditorTab::closeEditorTab(int32_t index)
    {
        widget(index)->deleteLater(); // Note: removeTab do not destroy the widget.
    }
    
    
    void EditorTab::tabNameEdited()
    {
        if (currentWidget() != nullptr)
        {
            currentWidget()->setFocus();
        }
    }
}
