#include "ThemeManager.h"

#include <QDebug>
#include <QFile>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace piper
{
    piper::ThemeManager& ThemeManager::instance()
    {
        static ThemeManager instance;
        return instance;
    }


    NodeTheme ThemeManager::getNodeTheme() const
    {
        return node_theme_;
    }


    AttributeTheme ThemeManager::getAttributeTheme() const
    {
        return attribute_theme_;
    }


    DataTypeTheme ThemeManager::getDataTypeTheme(QString const& dataType)
    {
        if (data_type_themes_.contains(dataType))
        {
            return data_type_themes_[dataType];
        }
        return data_type_themes_["default"];
    }


    bool ThemeManager::load(const QString& theme_filename)
    {
        QFile io(theme_filename);
        if (not io.open(QIODevice::ReadOnly))
        {
            qWarning() << "Can't open theme file" << theme_filename;
            return false;
        }


        QByteArray file_data = io.readAll();
        QJsonParseError error;
        QJsonDocument theme_document = QJsonDocument::fromJson(file_data, &error);
        if (error.error != QJsonParseError::NoError)
        {
            qWarning() << "Error while parsing theme file:" << error.errorString();
            return false;
        }

        if (not parseNode(theme_document.object()))
        {
            return false;
        }

        if (not parseAttribute(theme_document.object()))
        {
            return false;
        }

        if (not parseDataType(theme_document.object()))
        {
            return false;
        }

        return true;
    }


    bool ThemeManager::parseNode(QJsonObject const& json)
    {
        if (not (json.contains("node") and json["node"].isObject()))
        {
            qWarning() << "Can't parse node";
            return false;
        }
        QJsonObject node = json["node"].toObject();

        QJsonObject name = node["name"].toObject();
        node_theme_.name_font  = parseFont(name["font"].toObject());
        node_theme_.name_color = parseColor(name["font"].toObject());

        node_theme_.background = parseColor(node["background"].toObject());

        QJsonObject border = node["border"].toObject();
        node_theme_.border = parseColor(border["normal"].toObject());
        node_theme_.border_selected = parseColor(border["selected"].toObject());;

        QJsonObject type = node["type"].toObject();
        node_theme_.type_font  = parseFont(type["font"].toObject());
        node_theme_.type_color = parseColor(type["font"].toObject());
        node_theme_.type_background = parseColor(type["background"].toObject());;

        return true;
    }


    bool ThemeManager::parseAttribute(QJsonObject const& json)
    {
        if (not (json.contains("attribute") and json["attribute"].isObject()))
        {
            qWarning() << "Can't parse attribute";
            return false;
        }
        QJsonObject attribute = json["attribute"].toObject();

        attribute_theme_.background = parseColor(attribute["background"].toObject());
        attribute_theme_.background_alt = parseColor(attribute["background_alt"].toObject());

        auto parseMode = [this](QJsonObject const& json, AttributeTheme::Mode& mode)
        {
            mode.font = parseFont(json["font"].toObject());
            mode.font_color = parseColor(json["font"].toObject());

            QJsonObject connector = json["connector"].toObject();
            mode.connector.border_color = parseColor(json["connector"].toObject());
            mode.connector.border_width = connector["width"].toInt();
        };

        parseMode(attribute["minimize"].toObject(),  attribute_theme_.minimize);
        parseMode(attribute["normal"].toObject(),    attribute_theme_.normal);
        parseMode(attribute["highlight"].toObject(), attribute_theme_.highlight);

        return true;
    }


    bool ThemeManager::parseDataType(QJsonObject const& json)
    {
        if (not (json.contains("data_type") and json["data_type"].isObject()))
        {
            qWarning() << "Can't parse data type";
            return false;
        }
        QJsonObject data_type = json["data_type"].toObject();

        DataTypeTheme theme;
        theme.enable = parseColor(data_type["enable_default"].toObject());
        theme.disable = parseColor(data_type["disable"].toObject());
        theme.neutral = parseColor(data_type["neutral"].toObject());
        data_type_themes_["default"] = theme;

        QJsonObject enable_custom = data_type["enable_custom"].toObject();
        for (auto const& key : enable_custom.keys())
        {
            theme.enable = parseColor(enable_custom[key].toObject());
            data_type_themes_[key] = theme;
        }

        return true;
    }


    QColor ThemeManager::parseColor(QJsonObject const& json)
    {
        QJsonArray rgba = json["rgba"].toArray();
        return QColor{ rgba[0].toInt(), rgba[1].toInt(), rgba[2].toInt(), rgba[3].toInt()};
    }


    QFont ThemeManager::parseFont(QJsonObject const& json)
    {
        QString name = json["name"].toString();
        QString weight = json["weight"].toString();
        int size = json["size"].toInt();

        QFont::Weight qweight = QFont::Normal;
        if (weight == "Bold")
        {
            qweight = QFont::Bold;
        }
        else if (weight == "Medium")
        {
            qweight = QFont::Medium;
        }
        else if (weight == "Normal")
        {
            qweight = QFont::Normal;
        }
        else if (weight == "Light")
        {
            qweight = QFont::Light;
        }
        else
        {
            qWarning() << "Unknown font weight" << weight;
        }

        return QFont{ name, size, qweight };
    }

}
