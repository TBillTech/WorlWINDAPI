#pragma once

#include "backend.h"
#include "frontend_base.h"
#include <yaml-cpp/yaml.h>

// The YAMLMediator class is responsible for establishing a connection between two backends, 
// listening for changes in one backend and applying them to the other backend.
// Additionally, it converts the structure of the entire Backend tree to a single YAML node at the root of Backend yaml. 

// As usual, this class must ignore notifications that occur because of the cloning process itself.
// This class has a verioned flag that is turned on by default.  
// When the flag is set, notifications to update a node are only sent if the version of the node is higher.

// One should definitely use versioned flag true when the rate of changes is high.

// The class PropertySpecifier is used to accurately pinpoint a specific node in a tree, and a specific property in that node.
class PropertySpecifier {
public:
    PropertySpecifier(const std::string& node_label, const std::string& property_name,
                      const std::string& property_type)
        : nodeLabel_(node_label), propertyName_(property_name), propertyType_(property_type) {}
    PropertySpecifier(PropertySpecifier&& other) noexcept
        : nodeLabel_(std::move(other.nodeLabel_)), propertyName_(std::move(other.propertyName_)),
          propertyType_(std::move(other.propertyType_)) {}
    PropertySpecifier(const PropertySpecifier& other)
        : nodeLabel_(other.nodeLabel_), propertyName_(other.propertyName_), propertyType_(other.propertyType_) {}
    const std::string& getNodeLabel() const { return nodeLabel_; }
    const std::string& getPropertyName() const { return propertyName_; }
    const std::string& getPropertyType() const { return propertyType_; }
    bool operator==(const PropertySpecifier& other) const {
        return nodeLabel_ == other.nodeLabel_ &&
               propertyName_ == other.propertyName_ &&
               propertyType_ == other.propertyType_;
    }
    bool operator!=(const PropertySpecifier& other) const {
        return !(*this == other);
    }
    friend std::ostream& operator<<(std::ostream& os, const PropertySpecifier& specifier) {
        os << "Node: " << specifier.nodeLabel_ << ", Property: " << specifier.propertyName_
           << ", Type: " << specifier.propertyType_;
        return os;
    }
    friend std::istream& operator>>(std::istream& is, PropertySpecifier& specifier) {
        std::string node_label, property_name, property_type;
        is >> node_label >> property_name >> property_type;
        specifier.nodeLabel_ = node_label;
        specifier.propertyName_ = property_name;
        specifier.propertyType_ = property_type;
        return is;
    }
private:
    std::string nodeLabel_;
    std::string propertyName_;
    std::string propertyType_;
};

class YAMLMediator : public Frontend {
    public:
        YAMLMediator(const std::string& name, Backend& tree, Backend& yamlTree, const PropertySpecifier& specifier, bool initialize_from_yaml = true);
        ~YAMLMediator();

        // Frontend interface implementation
        std::string getName() const override { return name_; }
        std::string getType() const override { return "yaml_mediator"; }
        void start(Communication&, double, size_t) override { /* YAMLMediator starts automatically */ }
        void stop() override { /* YAMLMediator stops automatically */ }  
        bool isRunning() const override { return true; }
        
        std::vector<Backend*> getBackends() override {
            return {&backendTree_, &backendYAMLTree_};
        }

    private:
        std::string name_;
        Backend& backendTree_;
        Backend& backendYAMLTree_;
        PropertySpecifier const specifier_;
        atomic<bool> processingTree_;
        atomic<bool> processingYAMLTree_;
        YAML::Node yamlRepresentation_;
        std::string yamlTreeListenerName_;
        std::string treeListenerName_;
};