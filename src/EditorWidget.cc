#include "EditorWidget.h"
#include "PropertyDelegate.h"
#include "ui_EditorWidget.h"

#include "Scene.h"
#include "Node.h"
#include "Link.h"
#include "NodeCreator.h"

#include <cmath>
#include <QColorDialog>
#include <QDebug>

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
        QObject::connect(ui_->modes,    &QListView::clicked,       scene_, &Scene::onModeSelected);
        //QObject::connect(ui_->modes,    &QListView::customContextMenuRequested, scene_, &Scene::onModeSetDefault);
        QObject::connect(ui_->modes,    &QListView::doubleClicked, scene_, &Scene::onModeSetDefault);

        QHash<QString, QTreeWidgetItem*> allFrom;
        for (auto const& item : NodeCreator::instance().availableItems())
        {
            auto it = allFrom.find(item.from);
            if (it == allFrom.end())
            {
                it = allFrom.insert(item.from, new QTreeWidgetItem({item.from}));
            }

            (*it)->addChild(new QTreeWidgetItem({item.type, item.category}));
        }

        for (auto const& root : allFrom)
        {
            ui_->items->addTopLevelItem(root);
        }

        QObject::connect(ui_->items, &QTreeWidget::itemDoubleClicked,
        [&](QTreeWidgetItem* item)
        {
            if (item->child(0) != nullptr)
            {
                // Only leaf are relevants
                return;
            }

             // Alias for a easier reading
            QString const type = item->text(0);

            // Center of the view in the scene coordinates
            QPointF const scenePos = ui_->view->mapToScene(ui_->view->viewport()->rect().center());

            QString nextName = type + "_" + QString::number(scene_->nodes().size());
            Node* node = NodeCreator::instance().createItem(type, nextName, "", scenePos);
            if (node != nullptr)
            {
                scene_->addNode(node);
            }
        });
    }


    void EditorWidget::onExport(ExportBackend& backend)
    {
        scene_->onExport(backend);
    }


    void EditorWidget::onAddStage()
    {
        QColor nextColor = generateRandomColor();

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
        QModelIndex index = scene_->addMode(nextName);
        ui_->modes->setCurrentIndex(index);
        ui_->modes->edit(index);
    }


    void EditorWidget::onRmMode()
    {
        int row = ui_->modes->currentIndex().row();
        scene_->modes()->removeRows(row, 1);
    }


    QDataStream& operator<<(QDataStream& out, EditorWidget const& editor)
    {
        out << *editor.scene_;
        return out;
    }


    QDataStream& operator>>(QDataStream& in, EditorWidget& editor)
    {
        in >> *editor.scene_;
        return in;
    }


    void EditorWidget::loadJson(QJsonObject& json)
    {
        scene_->onImportJson(json);
        QModelIndex index = scene_->modes()->index(0, 0);
        if (index.isValid())
        {
            ui_->modes->setCurrentIndex(index);
            scene_->onModeSelected(index);
        }
    }
}
