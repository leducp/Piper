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
        scene_->loadJson(json);
    }
}
