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

    bool NodeRegistry::add(std::string library, NodeType type)
    {
        type.library = std::move(library);
        return add(type);
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
