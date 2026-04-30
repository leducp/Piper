#include "piper/registry.h"

namespace piper
{
    bool NodeRegistry::add(NodeType const& type)
    {
        auto const& key = type.type;
        if (types_.find(key) != types_.end())
        {
            return false;
        }
        types_.emplace(key, type);
        return true;
    }

    bool NodeRegistry::add(std::string library, NodeType const& type)
    {
        if (not add(type))
        {
            return false;
        }
        if (not library.empty())
        {
            library_of_.emplace(type.type, std::move(library));
        }
        return true;
    }

    std::string_view NodeRegistry::library_of(std::string_view type_name) const
    {
        auto it = library_of_.find(std::string(type_name));
        if (it == library_of_.end())
        {
            return {};
        }
        return it->second;
    }

    NodeType const* NodeRegistry::find(std::string_view type_name) const
    {
        auto it = types_.find(std::string(type_name));
        if (it == types_.end())
        {
            return nullptr;
        }
        return &it->second;
    }

    std::vector<NodeType const*> NodeRegistry::all() const
    {
        std::vector<NodeType const*> result;
        result.reserve(types_.size());
        for (auto const& kv : types_)
        {
            result.push_back(&kv.second);
        }
        return result;
    }
}
