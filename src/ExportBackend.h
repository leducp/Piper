#ifndef PIPER_EXPORT_BACKEND_H
#define PIPER_EXPORT_BACKEND_H

#include <QString>

namespace piper
{
    class ExportBackend
    {
    public:
        // init() is called befre anything else.
        virtual void init(QString const& filename) = 0;

        // finalize() is called when the export is finished.
        virtual void finalize() = 0;

        // Start a new pipeline
        virtual void startPipeline(QString const& pipelineName);

        // Called when the pipeline was fully exported.
        virtual void endPipeline();

        // Stages are written from first to last.
        virtual void writeStage(QString const& stage) = 0;

        // Each node is composed from one call for the metadata, and possible multiples call for its attributes
        virtual void writeNodeMetadata(QString const& type, QString const& name, QString const& stage) = 0;
        virtual void writeNodeAttribute(QString const& nodeName, QString const& name, QVariant const& data) = 0;

        // one call per link
        virtual void writeLink(QString const& from, QString const& output, QString const& to, QString const& input) = 0;

        // Mode
        virtual void writeMode(QString const& modeName, QString const& node, QString const& mode) = 0;
    };
}

#endif
