#pragma once

#include <string>
#include <vector>
#include <memory>
#include "backend.h"

class Communication;

/**
 * Frontend - Abstract base class for all WWATP frontend components
 * 
 * This base class provides a common interface for frontend components that
 * interact with Backend instances. Frontends can be mediators, servers, 
 * or other components that provide services on top of the Backend layer.
 * 
 * Common patterns across frontends:
 * - Take one or more Backend references
 * - Have string-based configuration (labels, URLs, etc.)  
 * - May have boolean flags for behavior control
 * - Provide lifecycle management (start/stop)
 */
class Frontend {
public:
    /**
     * Virtual destructor for proper cleanup
     */
    virtual ~Frontend() = default;

    /**
     * Get the name/identifier of this frontend instance
     * @return Frontend name
     */
    virtual std::string getName() const = 0;

    /**
     * Get the type of frontend (e.g., "cloning_mediator", "yaml_mediator", "http3_server")
     * @return Frontend type string
     */
    virtual std::string getType() const = 0;

    /**
     * Start the frontend (if applicable)
     * Some frontends may need explicit startup
     */
    virtual void start(Communication& connector, double time, size_t sleep_milli = 100) = 0;

    /**
     * Stop the frontend (if applicable)  
     * Some frontends may need explicit shutdown
     */
    virtual void stop() = 0;

    /**
     * Check if the frontend is currently running
     * @return True if running
     */
    virtual bool isRunning() const = 0;

    /**
     * Get the backends this frontend uses
     * @return Vector of backend pointers
     */
    virtual std::vector<Backend*> getBackends() = 0;

protected:
    /**
     * Protected constructor - only derived classes can create instances
     */
    Frontend() = default;

};
