#include <algorithm>

#include "piper/modes.h"

namespace piper
{
    std::vector<std::string> clean_modes(std::vector<std::string> const& modes)
    {
        std::vector<std::string> cleaned;
        for (auto const& mode : modes)
        {
            if (mode.empty() or mode == "disable")
            {
                continue;
            }
            if (std::find(cleaned.begin(), cleaned.end(), mode) == cleaned.end())
            {
                cleaned.push_back(mode);
            }
        }
        return cleaned;
    }

    bool is_clean_mode_list(std::vector<std::string> const& modes)
    {
        return clean_modes(modes) == modes;
    }

    std::vector<std::string> modes_of(Node const& node, NodeType const* type)
    {
        std::vector<std::string> candidates;
        if (type != nullptr)
        {
            candidates = type->modes;
        }
        candidates.insert(candidates.end(), node.modes.begin(), node.modes.end());

        std::vector<std::string> modes = clean_modes(candidates);
        if (modes.empty())
        {
            modes.push_back("enable");
        }
        modes.push_back("disable");
        return modes;
    }

    std::vector<std::string> modes_of(Node const& node, NodeRegistry const& reg)
    {
        return modes_of(node, reg.find(node.type));
    }

    bool accepts_mode(Node const& node, NodeRegistry const& reg, std::string_view mode)
    {
        if (mode.empty())
        {
            return false;
        }
        NodeType const* nt = reg.find(node.type);
        if (nt == nullptr)
        {
            return true;
        }
        auto const modes = modes_of(node, nt);
        return std::find(modes.begin(), modes.end(), mode) != modes.end();
    }

    void fill_missing_modes(Profile& profile, Graph const& graph, NodeRegistry const& reg)
    {
        for (auto const& node : graph.nodes())
        {
            auto const it = profile.per_node.find(node.id);
            if (it == profile.per_node.end() or it->second.empty())
            {
                profile.per_node[node.id] = modes_of(node, reg).front();
            }
        }
    }

    std::vector<Diagnostic> check_profile_modes(Graph const& graph, NodeRegistry const& reg)
    {
        std::vector<Diagnostic> diags;
        for (auto const& profile : graph.profiles())
        {
            for (auto const& node : graph.nodes())
            {
                auto const it = profile.per_node.find(node.id);
                if (it == profile.per_node.end() or it->second.empty())
                {
                    Diagnostic d;
                    d.kind    = Diagnostic::Kind::MissingMode;
                    d.message = "node '" + node.name + "' has no mode in profile '"
                              + profile.name + "'";
                    d.node_id = node.id;
                    diags.push_back(d);
                    continue;
                }
                if (accepts_mode(node, reg, it->second))
                {
                    continue;
                }
                Diagnostic d;
                d.kind    = Diagnostic::Kind::UnknownMode;
                d.message = "profile '" + profile.name + "' sets node '" + node.name
                          + "' to mode '" + it->second + "', which it does not declare";
                d.node_id = node.id;
                diags.push_back(d);
            }
        }
        return diags;
    }
}
