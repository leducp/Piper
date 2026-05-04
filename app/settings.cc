#include "piper/app/settings.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace piper::app
{
    std::string settings_path()
    {
        char const* xdg = std::getenv("XDG_CONFIG_HOME");
        if (xdg != nullptr and *xdg != '\0')
        {
            return std::string(xdg) + "/piper/settings.json";
        }
        char const* home = std::getenv("HOME");
        if (home != nullptr and *home != '\0')
        {
            return std::string(home) + "/.config/piper/settings.json";
        }
        return std::string{};
    }

    Settings load_settings()
    {
        Settings out;
        std::string const path = settings_path();
        if (path.empty())
        {
            return out;
        }
        std::ifstream in{path};
        if (not in.is_open())
        {
            return out;
        }
        std::ostringstream buf;
        buf << in.rdbuf();
        try
        {
            auto const doc = nlohmann::json::parse(buf.str());
            if (auto it = doc.find("font_path"); it != doc.end() and it->is_string())
            {
                out.font_path = it->get<std::string>();
            }
            if (auto it = doc.find("font_size"); it != doc.end() and it->is_number())
            {
                out.font_size = it->get<float>();
            }
            if (auto it = doc.find("window_x"); it != doc.end() and it->is_number_integer())
            {
                out.window_x = it->get<int>();
            }
            if (auto it = doc.find("window_y"); it != doc.end() and it->is_number_integer())
            {
                out.window_y = it->get<int>();
            }
            if (auto it = doc.find("window_w"); it != doc.end() and it->is_number_integer())
            {
                out.window_w = it->get<int>();
            }
            if (auto it = doc.find("window_h"); it != doc.end() and it->is_number_integer())
            {
                out.window_h = it->get<int>();
            }
        }
        catch (std::exception const& e)
        {
            std::fprintf(stderr, "settings: parse failed (%s)\n", e.what());
        }
        return out;
    }

    void save_settings(Settings const& s)
    {
        std::string const path = settings_path();
        if (path.empty())
        {
            return;
        }
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path(path).parent_path(), ec);
        if (ec)
        {
            std::fprintf(stderr, "settings: cannot create dir (%s)\n", ec.message().c_str());
            return;
        }

        nlohmann::json doc = nlohmann::json::object();
        // Read-modify-write so partial saves don't clobber other keys.
        std::ifstream existing{path};
        if (existing.is_open())
        {
            std::ostringstream buf;
            buf << existing.rdbuf();
            try
            {
                auto parsed = nlohmann::json::parse(buf.str());
                if (parsed.is_object())
                {
                    doc = std::move(parsed);
                }
            }
            catch (std::exception const&)
            {
            }
        }

        if (s.font_path.has_value())
        {
            doc["font_path"] = *s.font_path;
        }
        if (s.font_size.has_value())
        {
            doc["font_size"] = *s.font_size;
        }
        if (s.window_x.has_value()) { doc["window_x"] = *s.window_x; }
        if (s.window_y.has_value()) { doc["window_y"] = *s.window_y; }
        if (s.window_w.has_value()) { doc["window_w"] = *s.window_w; }
        if (s.window_h.has_value()) { doc["window_h"] = *s.window_h; }
        std::ofstream out{path};
        if (not out.is_open())
        {
            std::fprintf(stderr, "settings: cannot write %s\n", path.c_str());
            return;
        }
        out << doc.dump(2) << '\n';
    }
}
