#ifndef PIPER_COMMANDS_H
#define PIPER_COMMANDS_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "piper/command.h"
#include "piper/link.h"
#include "piper/node.h"
#include "piper/node_type.h"

namespace piper
{
    class Graph;

    class AddNodeCommand : public Command
    {
    public:
        AddNodeCommand(NodeType const& type,
                       std::string const& name,
                       std::string const& stage,
                       Point pos);

        void apply(Graph& g)  override;
        void revert(Graph& g) override;

        // Valid only after the first apply; returns invalid_node_id otherwise.
        NodeId node_id() const { return snapshot_.id; }

    private:
        NodeType type_;
        Node     snapshot_;
        bool     first_apply_{true};
    };

    class DeleteNodeCommand : public Command
    {
    public:
        explicit DeleteNodeCommand(NodeId id) : id_(id) {}

        void apply(Graph& g)  override;
        void revert(Graph& g) override;

    private:
        NodeId                          id_;
        std::optional<Node>             snapshot_;
        std::vector<Link>               incident_links_;
    };

    class MoveNodeCommand : public Command
    {
    public:
        MoveNodeCommand(NodeId id, Point new_pos) : id_(id), new_pos_(new_pos) {}

        void apply(Graph& g)         override;
        void revert(Graph& g)        override;
        bool try_merge(Command const& next) override;

    private:
        NodeId               id_;
        Point                new_pos_;
        std::optional<Point> old_pos_;
    };

    class RenameNodeCommand : public Command
    {
    public:
        RenameNodeCommand(NodeId id, std::string const& new_name)
            : id_(id), new_name_(new_name) {}

        void apply(Graph& g)  override;
        void revert(Graph& g) override;

    private:
        NodeId                     id_;
        std::string                new_name_;
        std::optional<std::string> old_name_;
    };

    class SetNodeStageCommand : public Command
    {
    public:
        SetNodeStageCommand(NodeId id, std::string const& new_stage)
            : id_(id), new_stage_(new_stage) {}

        void apply(Graph& g)  override;
        void revert(Graph& g) override;

    private:
        NodeId                     id_;
        std::string                new_stage_;
        std::optional<std::string> old_stage_;
    };

    class CreateLinkCommand : public Command
    {
    public:
        CreateLinkCommand(PinRef const& from, PinRef const& to, std::string const& data_type);

        void apply(Graph& g)  override;
        void revert(Graph& g) override;

        // Valid only after the first apply; returns invalid_link_id otherwise.
        LinkId link_id() const { return snapshot_.id; }

    private:
        PinRef      from_;
        PinRef      to_;
        std::string data_type_;
        Link        snapshot_;
        bool        first_apply_{true};
    };

    class DeleteLinkCommand : public Command
    {
    public:
        explicit DeleteLinkCommand(LinkId id) : id_(id) {}

        void apply(Graph& g)  override;
        void revert(Graph& g) override;

    private:
        LinkId              id_;
        std::optional<Link> snapshot_;
    };

    class SetAttributeValueCommand : public Command
    {
    public:
        SetAttributeValueCommand(NodeId id,
                                 std::string const& attr_name,
                                 std::string const& new_value)
            : id_(id), attr_name_(attr_name), new_value_(new_value) {}

        void apply(Graph& g)         override;
        void revert(Graph& g)        override;
        bool try_merge(Command const& next) override;

    private:
        NodeId                     id_;
        std::string                attr_name_;
        std::string                new_value_;
        std::optional<std::string> old_value_;
    };

    class SetAttributeStagesCommand : public Command
    {
    public:
        SetAttributeStagesCommand(NodeId id,
                                  std::string const& attr_name,
                                  std::vector<std::string> const& new_stages)
            : id_(id), attr_name_(attr_name), new_stages_(new_stages) {}

        void apply(Graph& g)         override;
        void revert(Graph& g)        override;
        bool try_merge(Command const& next) override;

    private:
        NodeId                                  id_;
        std::string                             attr_name_;
        std::vector<std::string>                new_stages_;
        std::optional<std::vector<std::string>> old_stages_;
    };

    class CompositeCommand : public Command
    {
    public:
        explicit CompositeCommand(std::vector<std::unique_ptr<Command>> children)
            : children_(std::move(children)) {}

        void apply(Graph& g)  override;
        void revert(Graph& g) override;

        std::size_t size() const { return children_.size(); }

    private:
        std::vector<std::unique_ptr<Command>> children_;
    };
}

#endif
