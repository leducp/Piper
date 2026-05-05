#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "piper/theme.h"

#include "diagnostic_helpers.h"
#include "piper/rgba_io.h"

namespace piper
{
    using nlohmann::json;
    constexpr int theme_format_version = 2;

    void parse_color_field(json const& obj,
                           char const* key,
                           rgba& out,
                           std::vector<Diagnostic>& diags)
    {
        auto it = obj.find(key);
        if (it == obj.end())
        {
            return;
        }
        if (not it->is_string())
        {
            diags.push_back(schema_error(
                std::string("expected string color for '") + key + "'"));
            return;
        }
        auto parsed = parse_rgba(it->get<std::string>());
        if (parsed.has_value())
        {
            out = *parsed;
        }
        else
        {
            diags.push_back(schema_error(
                std::string("malformed color for '") + key + "'"));
        }
    }

    void require_object(json const& doc,
                        char const* section,
                        std::vector<Diagnostic>& diags)
    {
        auto it = doc.find(section);
        if (it != doc.end() and not it->is_object())
        {
            diags.push_back(schema_error(
                std::string("'") + section + "' must be an object"));
        }
    }

    void parse_color_map(json const& obj,
                         std::unordered_map<std::string, rgba>& out,
                         std::string const& kind_label,
                         std::vector<Diagnostic>& diags)
    {
        for (auto const& [key, value] : obj.items())
        {
            if (not value.is_string())
            {
                continue;
            }
            auto parsed = parse_rgba(value.get<std::string>());
            if (parsed.has_value())
            {
                out[key] = *parsed;
            }
            else
            {
                diags.push_back(schema_error(
                    "malformed color for " + kind_label + " '" + key + "'"));
            }
        }
    }

    ThemeLoadResult load_theme_from_string(std::string_view text)
    {
        json doc;
        try
        {
            doc = json::parse(text);
        }
        catch (json::parse_error const& e)
        {
            throw std::runtime_error(std::string("malformed theme JSON: ") + e.what());
        }

        int version = doc.value("version", 0);
        if (version != theme_format_version)
        {
            throw std::runtime_error(
                "unsupported theme version " + std::to_string(version)
                + " (expected " + std::to_string(theme_format_version) + ")");
        }

        ThemeLoadResult result;
        Theme& t = result.theme;

        // Built-in mode colors: present even when the JSON omits them.
        // JSON values for the same labels override these.
        t.mode_colors["enable"]  = rgba::from_components(0xFF, 0xFF, 0xFF, 0xFF);
        t.mode_colors["disable"] = rgba::from_components(0x66, 0x66, 0x66, 0xFF);

        for (char const* section : { "canvas", "node", "link", "types", "modes" })
        {
            require_object(doc, section, result.diagnostics);
        }

        if (auto canvas_it = doc.find("canvas"); canvas_it != doc.end() and canvas_it->is_object())
        {
            parse_color_field(*canvas_it, "bg", t.canvas_bg, result.diagnostics);
            if (auto grid_it = canvas_it->find("grid"); grid_it != canvas_it->end() and grid_it->is_object())
            {
                parse_color_field(*grid_it, "color", t.grid_line, result.diagnostics);
                t.grid_spacing = grid_it->value("spacing", t.grid_spacing);
            }
        }

        if (auto node_it = doc.find("node"); node_it != doc.end() and node_it->is_object())
        {
            parse_color_field(*node_it, "header_default", t.node_default_header, result.diagnostics);
            parse_color_field(*node_it, "body_default",   t.node_default_body,   result.diagnostics);
            t.node_rounding            = node_it->value("rounding",            t.node_rounding);
            t.node_body_alpha_disabled = node_it->value("body_alpha_disabled", t.node_body_alpha_disabled);
            t.pin_alpha_inactive       = node_it->value("pin_alpha_inactive",  t.pin_alpha_inactive);
        }

        if (auto link_it = doc.find("link"); link_it != doc.end() and link_it->is_object())
        {
            t.link_thickness       = link_it->value("thickness",       t.link_thickness);
            t.link_bezier_strength = link_it->value("bezier_strength", t.link_bezier_strength);
            parse_color_field(*link_it, "invalid", t.link_invalid, result.diagnostics);
        }

        if (auto font_it = doc.find("font"); font_it != doc.end() and font_it->is_object())
        {
            t.font_path = font_it->value("path", t.font_path);
            t.font_size = font_it->value("size", t.font_size);
        }

        if (auto types_it = doc.find("types"); types_it != doc.end() and types_it->is_object())
        {
            parse_color_map(*types_it, t.type_colors, "type", result.diagnostics);
        }

        if (auto modes_it = doc.find("modes"); modes_it != doc.end() and modes_it->is_object())
        {
            parse_color_map(*modes_it, t.mode_colors, "mode", result.diagnostics);
        }

        return result;
    }

    ThemeLoadResult load_theme(std::string_view path)
    {
        std::ifstream in{std::string(path)};
        if (not in.is_open())
        {
            throw std::runtime_error("cannot open theme: " + std::string(path));
        }
        std::ostringstream buf;
        buf << in.rdbuf();
        return load_theme_from_string(buf.str());
    }
}
