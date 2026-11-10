#ifndef PIPER_NODE_H
#define PIPER_NODE_H

#include <stdint.h>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "piper/attribute.h"

namespace piper
{
    using NodeId = uint64_t;
    constexpr NodeId invalid_node_id = 0;

    struct Point
    {
        float x{0.0f};
        float y{0.0f};

        constexpr bool operator==(Point const& other) const
        {
            return x == other.x and y == other.y;
        }

        constexpr bool operator!=(Point const& other) const
        {
            return not (*this == other);
        }
    };

    using MemberValues = std::map<std::string, std::string>;
    using LabelMemberValues = std::map<std::string, MemberValues>;

    struct Node
    {
        NodeId      id{invalid_node_id};
        std::string type;
        std::string name;
        std::string stage;
        // Free-form per-instance note shown in the inspector + on hover.
        std::string note;
        Point       pos;
        std::vector<Attribute> attrs;

        bool members_per_label{false};
        LabelMemberValues label_members;

        MemberValues shared_members() const
        {
            MemberValues values;
            for (auto const& attr : attrs)
            {
                if (attr.role == AttributeSpec::Role::Member)
                {
                    values.emplace(attr.name, attr.value);
                }
            }
            return values;
        }

        // Per-label mode always selects a complete, independently stored set.
        MemberValues members_for_label(std::string_view label) const
        {
            if (not members_per_label)
            {
                return shared_members();
            }
            auto it = label_members.find(std::string(label));
            if (it == label_members.end())
            {
                throw std::out_of_range("no member set for label '" + std::string(label) + "'");
            }
            return it->second;
        }

        // Initialize new labels once, copying values rather than sharing them.
        bool initialize_member_labels(std::vector<std::string> const& labels)
        {
            if (not members_per_label)
            {
                return false;
            }
            bool changed = false;
            auto const initial = shared_members();
            for (auto const& label : {"", "enable", "disable"})
            {
                changed = label_members.emplace(label, initial).second or changed;
            }
            for (auto const& label : labels)
            {
                changed = label_members.emplace(label, initial).second or changed;
            }
            return changed;
        }

        std::vector<std::string> member_configuration_errors() const
        {
            std::vector<std::string> errors;
            auto const shared = shared_members();
            if (not members_per_label and not label_members.empty())
            {
                errors.push_back("shared member configuration cannot contain per-label sets");
            }
            if (members_per_label)
            {
                for (auto const& label : {"", "enable", "disable"})
                {
                    if (label_members.count(label) == 0)
                    {
                        errors.push_back("missing member set for label '" + std::string(label) +
                                         "'");
                    }
                }
            }
            for (auto const& [label, values] : label_members)
            {
                for (auto const& [member_name, _] : shared)
                {
                    if (values.count(member_name) == 0)
                    {
                        errors.push_back("label '" + label + "' missing member '" + member_name +
                                         "'");
                    }
                }
                for (auto const& [member_name, _] : values)
                {
                    if (shared.count(member_name) == 0)
                    {
                        errors.push_back("label '" + label + "' has unknown member '" +
                                         member_name + "'");
                    }
                }
            }
            return errors;
        }

        Attribute const* find_attr(std::string_view attr_name) const
        {
            for (auto const& a : attrs)
            {
                if (a.name == attr_name)
                {
                    return &a;
                }
            }
            return nullptr;
        }
    };
}

#endif
