#include "JsonExport.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>

namespace piper
{
    void JsonExport::init(QString const&)
    {

    }

    void JsonExport::finalize(QString const& filename)
    {
        QJsonDocument document(root_);

        QFile io(filename);
        if (not io.open(QIODevice::WriteOnly))
        {
            qDebug() << "Error while opening" << io.fileName();
            return;
        }

        io.write(document.toJson());
    }

    void JsonExport::startPipeline(QString const&)
    {
        pipeline_ = QJsonObject(); // cleanup
        nodes_ = QJsonObject();
        links_ = QJsonArray();
        modes_ = QJsonObject();
    }

    void JsonExport::endPipeline(QString const& pipelineName)
    {
        pipeline_["Nodes"] = nodes_;
        pipeline_["Links"] = links_;
        pipeline_["Modes"] = modes_;
        root_[pipelineName] = pipeline_;
    }


    void JsonExport::writeStages(QVector<QString> const& stages)
    {
        QJsonArray stagesArray;
        for (auto const& stage : stages)
        {
            stagesArray.append(stage);
        }
        pipeline_["Stages"] = stagesArray;
    }


    void JsonExport::writeNode(QString const& type, QString const& name, QString const& stage, QHash<QString, QVariant> const& attributes)
    {
        QJsonObject node;
        node["type"] = type;
        node["stage"] = stage;

        for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it)
        {
            if (it.key() == "type")  { qWarning() << "type is a reerved attribute. Skipping.";  continue; }
            if (it.key() == "stage") { qWarning() << "stage is a reerved attribute. Skipping."; continue; }
            node[it.key()] = QJsonValue::fromVariant(it.value());
        }

        nodes_[name] = node;
    }


    void JsonExport::writeLink(QString const& from, QString const& output, QString const& to, QString const& input, QString const& type)
    {
        QJsonObject link;
        link["from"] = from;
        link["out"] = output;
        link["to"] = to;
        link["in"] = input;
        link["type"] = type;
        links_.append(link);
    }


    void JsonExport::writeMode(QString const& name, QHash<QString, Mode> const& config)
    {
        QJsonObject mode;
        QJsonObject configuration;
        mode["default"] = "Enable";
        for (auto it = config.constBegin(); it != config.constEnd(); ++it)
        {
            QString modeString;
            switch (it.value())
            {
                case Mode::enable:  { continue; }
                case Mode::disable: { modeString = "Disable"; break; }
                case Mode::neutral: { modeString = "Neutral"; break; }
                default: { continue; }
            }
            configuration[it.key()] = modeString;
        }
        mode["configuration"] = configuration;
        modes_[name] = mode;
    }

    void JsonExport::writeDefaultMode(QString const& name)
    {
        QJsonValue defaultMode = name;
        modes_["default"] = defaultMode;
    }
}
