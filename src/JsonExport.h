#ifndef PIPER_JSON_EXPORT_H
#define PIPER_JSON_EXPORT_H

#include "ExportBackend.h"

#include <QJsonObject>
#include <QJsonArray>

namespace piper
{
    class JsonExport : public ExportBackend
    {
    public:
        JsonExport() = default;
        virtual ~JsonExport() = default;

        // init() is called befre anything else.
        void init(QString const& filename) override;

        // finalize() is called when the export is finished.
        void finalize(QString const& filename) override;

        // Start a new pipeline
        void startPipeline(QString const&) override;

        // Called when the pipeline was fully exported.
        void endPipeline(QString const& pipelineName) override;

        // Stages
        void writeStages(QVector<QString> const& stages) override;

        // Each node is composed of its metadata and a map attributes/value
        void writeNode(QString const& type, QString const& name, QString const& stage, QHash<QString, QVariant> const& attributes) override;

        // one call per link
        void writeLink(QString const& from, QString const& output, QString const& to, QString const& input, QString const& type) override;

        // Mode
        void writeMode(QString const& name, QHash<QString, Mode> const& config) override;

    private:
        QJsonObject root_;
        QJsonObject pipeline_;
        QJsonObject nodes_;
        QJsonArray links_;
        QJsonObject modes_;
    };
}

#endif
