#include <signal.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "piper/app/autosave.h"

#include "piper/serialize_v2.h"

namespace piper::studio
{
    std::string autosave_dir()
    {
        char const* xdg = std::getenv("XDG_DATA_HOME");
        if (xdg != nullptr and *xdg != '\0')
        {
            return std::string(xdg) + "/piper/autosave";
        }
        char const* home = std::getenv("HOME");
        if (home != nullptr and *home != '\0')
        {
            return std::string(home) + "/.local/share/piper/autosave";
        }
        return std::string{};
    }

    void autosave_write(Document& doc)
    {
        std::string const dir = autosave_dir();
        if (dir.empty())
        {
            return;
        }
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec)
        {
            return;
        }
        // PID in the name keeps concurrent editors from clobbering
        // each other's session-<n> files.
        std::string const path = dir + "/session-" + std::to_string(::getpid())
                               + "-" + std::to_string(doc.session_id) + ".piper";
        std::string const tmp  = path + ".tmp";
        std::string const json = piper::v2::serialize(doc.graph, doc.pipeline_name);
        {
            std::ofstream f(tmp);
            if (not f.is_open())
            {
                return;
            }
            f << json;
            f.flush();
            if (not f)
            {
                // Don't let the rename clobber the previous good autosave.
                std::filesystem::remove(tmp, ec);
                return;
            }
        }
        std::filesystem::rename(tmp, path, ec);
        if (ec)
        {
            std::filesystem::remove(tmp, ec);
            return;
        }
        doc.autosave_path    = path;
        doc.last_autosave_at = std::chrono::steady_clock::now();
    }

    void autosave_remove(Document& doc)
    {
        if (doc.autosave_path.empty())
        {
            return;
        }
        std::error_code ec;
        std::filesystem::remove(doc.autosave_path, ec);
        doc.autosave_path.clear();
    }

    bool process_is_alive(long pid)
    {
        if (::kill(pid_t(pid), 0) != 0)
        {
            return false;
        }
        // kill(pid, 0) also succeeds for a zombie; read the proc state so a
        // crashed-but-unreaped session's autosave is still offered.
        std::ifstream status{"/proc/" + std::to_string(pid) + "/status"};
        if (not status)
        {
            return true;
        }
        std::string line;
        while (std::getline(status, line))
        {
            if (line.rfind("State:", 0) == 0)
            {
                return line.find('Z') == std::string::npos;
            }
        }
        return true;
    }

    std::vector<std::string> scan_autosave_dir()
    {
        std::vector<std::string> out;
        std::string const dir = autosave_dir();
        if (dir.empty())
        {
            return out;
        }
        std::error_code ec;
        if (not std::filesystem::is_directory(dir, ec))
        {
            return out;
        }
        for (auto const& entry : std::filesystem::directory_iterator(dir, ec))
        {
            if (not entry.is_regular_file()
                or entry.path().extension() != ".piper")
            {
                continue;
            }
            // session-<pid>-<id>.piper: skip files still owned by a
            // live process. Legacy session-<id>.piper (no pid part)
            // is always offered.
            std::string const stem = entry.path().stem().string();
            constexpr std::string_view prefix = "session-";
            if (stem.starts_with(prefix))
            {
                std::string const rest = stem.substr(prefix.size());
                std::size_t const dash = rest.find('-');
                if (dash != std::string::npos)
                {
                    char* end = nullptr;
                    long const pid = std::strtol(rest.c_str(), &end, 10);
                    if (pid > 0
                        and end == rest.c_str() + dash
                        and process_is_alive(pid))
                    {
                        continue;
                    }
                }
            }
            out.push_back(entry.path().string());
        }
        return out;
    }
}
