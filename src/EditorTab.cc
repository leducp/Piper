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
    { 

    }
        
        
    void TabHeaderEdit::mousePressEvent(QMouseEvent *event) 
    {
        emit getFocus(this);
        QLineEdit::mousePressEvent(event);
        QWidget::mousePressEvent(event); // let the parent do its work too !
    }


    TabNameValidator::TabNameValidator(QObject* parent) 
        : QValidator(parent) 
        , editors_{}
    { 
        
    }
    
    
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
        
        QStringList forbiddenStrings;
        for (auto const& editor : editors_)
        {
            if (editor == filteredEditor_)
            {
                // Do not forbid editor to write itself.
                continue;
            }
            forbiddenStrings << editor->text();
        }
        
        // Refuse already defined name.
        if (forbiddenStrings.contains(input))
        {
            return QValidator::Intermediate;
        }
        
        return QValidator::Acceptable;
    }
        
        
    void TabNameValidator::fixup(QString& input) const
    {
        QStringList forbiddenStrings;
        for (auto const& editor : editors_)
        {
            forbiddenStrings << editor->text();
        }
        
        QString fixedInput = input;
        for (int32_t i = 0; forbiddenStrings.contains(fixedInput); ++i)
        {
            fixedInput = input + " (" + QString::number(i) + ")";
        }
        input = fixedInput;
    }
        
        
    void TabNameValidator::addEditor(TabHeaderEdit* editor)
    {
        editors_.append(editor);
        emit changed();
    }
    
    
    void TabNameValidator::removeEditor(TabHeaderEdit* editor)
    {
        editors_.removeAll(editor);
        emit changed();
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
    
    
    void EditorTab::createNewEditorTab()
    {
        EditorWidget* editor = new EditorWidget();
        int32_t index = addTab(editor, "");
        
        QString defaultText = "unamed pipeline";
        tabNameValidator_->fixup(defaultText);
        
        TabHeaderEdit* edit = new TabHeaderEdit(defaultText);
        edit->setFrame(false);
        edit->setStyleSheet("QLineEdit { background:transparent; }");
        edit->setValidator(tabNameValidator_);
        tabNameValidator_->addEditor(edit);
        tabBar()->setTabButton(index, QTabBar::LeftSide, edit);
        QObject::connect(edit, &TabHeaderEdit::getFocus, tabNameValidator_, &TabNameValidator::updateHeaderEditFilter);
        QObject::connect(edit, &QLineEdit::editingFinished, this, &EditorTab::tabNameEdited);
    }

    
    void EditorTab::closeEditorTab(int32_t index)
    {
        tabNameValidator_->removeEditor(static_cast<TabHeaderEdit*>(tabBar()->tabButton(index, QTabBar::LeftSide)));
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
