#ifndef PIPER_SCENE_H
#define PIPER_SCENE_H

#include <QGraphicsScene>
#include <QStandardItemModel>
#include <QVector>
#include <QJsonObject>

namespace piper
{
    class Link;
    class Node;
    class ExportBackend;

    class Scene : public QGraphicsScene
    {
        Q_OBJECT

    public:
        Scene(QObject *parent = nullptr);
        virtual ~Scene();

        void resetStagesColor();
        void updateStagesColor(QString const& stage, QColor const& color);

        void addNode(Node* node);
        void removeNode(Node* node);
        QVector<Node*> const& nodes() const { return nodes_; }

        void addLink(Link* link);
        void removeLink(Link* link);
        QVector<Link*> const& links() const { return links_; }
        void connect(QString const& from, QString const& out, QString const& to, QString const& in);

        QModelIndex addMode(QString const& name);

        QStandardItemModel* stages() const { return stages_; }
        QStandardItemModel* modes()  const { return modes_;  }

        void onExport(ExportBackend& backend);
        void onImportJson(QJsonObject& json);

    public slots:
        void onModeSelected(QModelIndex const& index);
        void onModeSetDefault(QModelIndex const& index);
        void onModeRemoved();

        void onStageUpdated();

    protected:
        void drawBackground(QPainter *painter, const QRectF &rect) override;
        void keyReleaseEvent(QKeyEvent *keyEvent) override;

    private:
        void loadNodesJson(QJsonObject& steps);
        void loadLinksJson(QJsonArray& links);
        void loadModesJson(QJsonObject& modes);
        void placeNodesDefaultPosition();

        QVector<Node*> nodes_;
        QVector<Link*> links_;

        QVector<QString> nodes_import_errors_;
        QVector<QString> links_import_errors_;

        QStandardItemModel* stages_;
        QStandardItemModel* modes_;
    };

    QDataStream& operator<<(QDataStream& out, Scene const& scene);
    QDataStream& operator>>(QDataStream& in,  Scene& scene);
    QColor generateRandomColor();
}

#endif

