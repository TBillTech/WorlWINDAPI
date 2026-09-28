#pragma once

#include "backend.h"
#include "frontend_base.h"

// The CloningMediator class is responsible for establishing a connection between two backends, 
// listening for changes in one backend and applying them to the other backend.

// The main complexity comes from ignoring notifications that occur because of the cloning process itself.
// For example, when a Backend A callback is triggered by a change in Backend A, and that change is written to Backend B,
// we want to ignore the notification that is triggered by Backend B.  Moreover, this class has a verioned flag that is
// turned on by default.  When the flag is set, notifications to update a node are only sent if the version of the node is higher.

// One should definitely use versioned flag true when the rate of changes is high.

class CloningMediator : public Frontend {
    public:
        CloningMediator(const std::string& name, Backend& a, Backend& b, bool versioned = true);
        ~CloningMediator();

        // Frontend interface implementation
        std::string getName() const override { return name_; }
        std::string getType() const override { return "cloning_mediator"; }
        void start(Communication&, double, size_t) override { /* CloningMediator starts automatically */ }
        void stop() override { /* CloningMediator stops automatically */ }  
        bool isRunning() const override { return true; }
        
        std::vector<Backend*> getBackends() override {
            return {&backendA_, &backendB_};
        }

    private:
        std::string name_;
        Backend& backendA_;
        Backend& backendB_;
        bool versioned_ = true;
        atomic<bool> processingA_;
        atomic<bool> processingB_;
};