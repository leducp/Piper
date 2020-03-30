#include "EditorWidget.h"
#include "PropertyDelegate.h"
#include "ui_EditorWidget.h"

#include "Scene.h"
#include "Node.h"
#include "Link.h"

#include <cmath>
#include <QColorDialog>

namespace piper
{
    EditorWidget::EditorWidget(QWidget* parent)
        : QWidget(parent)
        , ui_(new Ui::EditorWidget)
        , scene_(new Scene(this))
    {
        srand(time(0));

        ui_->setupUi(this);
        ui_->view->setScene(scene_);

        QObject::connect(ui_->stage_add,   &QPushButton::clicked, this, &EditorWidget::onAddStage);
        QObject::connect(ui_->stage_rm,    &QPushButton::clicked, this, &EditorWidget::onRmStage);
        QObject::connect(ui_->stage_color, &QPushButton::clicked, this, &EditorWidget::onColorStage);

        ui_->stages->setModel(scene_->stages());
        ui_->stages->setEditTriggers(QAbstractItemView::AnyKeyPressed |
                                     QAbstractItemView::DoubleClicked);
        ui_->stages->setDragDropMode(QAbstractItemView::InternalMove);

        ui_->modes->setModel(scene_->modes());
        ui_->modes->setEditTriggers(QAbstractItemView::AnyKeyPressed |
                                    QAbstractItemView::DoubleClicked);
        ui_->modes->setDragDropMode(QAbstractItemView::InternalMove);
        QObject::connect(ui_->mode_add, &QPushButton::clicked, this, &EditorWidget::onAddMode);
        QObject::connect(ui_->mode_rm,  &QPushButton::clicked, this, &EditorWidget::onRmMode);
        QObject::connect(ui_->modes,    &QListView::clicked,   scene_, &Scene::onModeSelected);
    }


    void EditorWidget::onAddStage()
    {
        // procedural color generator: the gold ratio
        static double nextColorHue = 1.0 / (rand() % 100); // don't need a proper random here
        constexpr double golden_ratio_conjugate = 0.618033988749895; // 1 / phi
        nextColorHue += golden_ratio_conjugate;
        nextColorHue = std::fmod(nextColorHue, 1.0);

        QColor nextColor;
        nextColor.setHsvF(nextColorHue, 0.5, 0.99);

        QString nextName = "stage" + QString::number(scene_->stages()->rowCount());

        // Add item
        QStandardItem* item = new QStandardItem();
        item->setData(nextColor, Qt::DecorationRole);
        item->setData(nextName, Qt::DisplayRole);
        item->setDropEnabled(false);;
        scene_->stages()->appendRow(item);

        // Enable item selection and put it edit mode
        QModelIndex index = scene_->stages()->indexFromItem(item);
        ui_->stages->setCurrentIndex(index);
        ui_->stages->edit(index);
    }


    void EditorWidget::onRmStage()
    {
        int row = ui_->stages->currentIndex().row();
        scene_->stages()->removeRows(row, 1);
        scene_->onStageUpdated();
    }


    void EditorWidget::onColorStage()
    {
        QModelIndex index = ui_->stages->currentIndex();
        QColor current = scene_->stages()->data(index, Qt::DecorationRole).value<QColor>();

        QColor newColor = QColorDialog::getColor(current);
        scene_->stages()->setData(index, newColor, Qt::DecorationRole);
    }


    void EditorWidget::onAddMode()
    {
        QString nextName = "mode" + QString::number(scene_->modes()->rowCount());

        // Add item
        QStandardItem* item = new QStandardItem();
        item->setData(nextName, Qt::DisplayRole);
        item->setDropEnabled(false);;
        scene_->modes()->appendRow(item);

        // Enable item selection and put it edit mode
        QModelIndex index = scene_->modes()->indexFromItem(item);
        ui_->modes->setCurrentIndex(index);
        ui_->modes->edit(index);
        scene_->onModeSelected(index);
    }


    void EditorWidget::onRmMode()
    {
        int row = ui_->modes->currentIndex().row();
        scene_->modes()->removeRows(row, 1);
    }


    // TODO: seems that the scene shall be responsible of this stuff
    QDataStream& operator<<(QDataStream& out, EditorWidget const& editor)
    {
        // save stages.
        out << editor.scene_->stages()->rowCount();
        for (int i = 0; i < editor.scene_->stages()->rowCount(); ++i)
        {
            out << *editor.scene_->stages()->item(i, 0);
        }

        // save nodes.
        out << editor.scene_->nodes().size();
        for (auto const& node : editor.scene_->nodes())
        {
            out << *node;
        }

        // save links.
        out << editor.scene_->links().size();
        for (auto const& link : editor.scene_->links())
        {
            out << static_cast<Node*>(link->from()->parentItem())->name() << link->from()->name();
            out << static_cast<Node*>(link->to()->parentItem())->name()   << link->to()->name();
        }

        return out;
    }


    // TODO: seems that the scene shall be responsible of this stuff
    QDataStream& operator>>(QDataStream& in, EditorWidget& editor)
    {
        // Load stages.
        int stageCount;
        in >> stageCount;
        for (int i = 0; i < stageCount; ++i)
        {
            QStandardItem* item = new QStandardItem();
            in >> *item;
            editor.scene_->stages()->setItem(i, item);
        }

        // Load nodes.
        int nodeCount;
        in >> nodeCount;
        for (int i = 0; i < nodeCount; ++i)
        {
            Node* node = new Node();
            in >> *node;
            editor.scene_->addNode(node);
        }

        // Load links.
        int linkCount;
        in >> linkCount;
        for (int i = 0; i < linkCount; ++i)
        {
            QString from, output;
            in >> from >> output;

            QString to, input;
            in >> to >> input;

            editor.scene_->connect(from, output, to, input);
        }

        editor.scene_->onStageUpdated();

        return in;
    }
}
