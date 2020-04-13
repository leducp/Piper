#ifndef PIPER_EXPORT_BACKEND_H
#define PIPER_EXPORT_BACKEND_H

#include <QString>
#include <QVector>
#include <QVariant>

#include "Types.h"

namespace piper
{
    class ExportBackend
    {
    public:
        // init() is called befre anything else.
        virtual void init(QString const& filename) = 0;

        // finalize() is called when the export is finished.
        virtual void finalize(QString const& filename) = 0;

        // Start a new pipeline
        virtual void startPipeline(QString const& pipelineName) = 0;

        // Called when the pipeline was fully exported.
        virtual void endPipeline(QString const& pipelineName) = 0;

        // Stages are written from first to last.
        virtual void writeStages(QVector<QString> const& stages) = 0;

        // Each node is composed of its metadata and a map attributes/value
        virtual void writeNode(QString const& type, QString const& name, QString const& stage, QHash<QString, QVariant> const& attributes) = 0;

        // one call per link
        virtual void writeLink(QString const& from, QString const& output, QString const& to, QString const& input, QString const& type) = 0;

        // one call per mode
        virtual void writeMode(QString const& name, QHash<QString, Mode> const& config) = 0;

        // one call per pipeline.
        virtual void writeDefaultMode(QString const& name) = 0;
    };
}

#endif
