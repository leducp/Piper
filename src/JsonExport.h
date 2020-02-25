#ifndef PIPER_JSON_EXPORT_H
#define PIPER_JSON_EXPORT_H

#include "Editor.h"

#include <QJsonObject>

namespace piper
{
    class JsonExport : public ExportBackend
    {
    public:
        JsonExport() = default;
        virtual ~JsonExport() = default;
        
        // init() is called befre anything else.
        void init() override;
        
        // Stages are written from first to last.
        void writeStage(QString const& stage) override;
        
        // Each node is composed from one call for the metadata, and possible multiples call for its attributes
        void writeNodeMetadata(QString const& type, QString const& name, QString const& stage) override;
        void writeNodeAttribute(QString const& nodeName, QString const& name, QVariant const& data) override;
        
        // one call per link
        void writeLink(QString const& from, QString const& output, QString const& to, QString const& input) override;
        
        // finalize() is called when the export is finished.
        void finalize(QString const& filename) override;
        
    private:
        int stage_number_;
        QJsonObject object_;
        QString filename_;
    };
}

#endif
