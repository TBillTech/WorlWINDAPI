# Resolution Backend Architecture

The Resolution Backend is the public backend facade for the resolution system.
Internally, it coordinates four focused components: the Cache Backend, Strategy
Backend, Planning Backend, and Audit Backend.

The Resolution Backend is therefore both:

- A backend that exposes the system's resolution operations to its callers
- A coordinator that orchestrates the internal resolution components

## Cache Backend

Stores resolved values and provides:

- Fast lookup
- Storage and retrieval of resolved values
- Cache entry replacement and removal

The Cache Backend does not track dependencies or decide when a value is stale.

## Strategy Backend

Stores resolution strategies, including:

- Declarative rules
- Templates
- Heuristics
- Algorithms
- Fallback logic

The Strategy Backend selects or applies strategies when requested by the
Resolution Coordinator. It does not manage cached values or audit resolution
events.

## Planning Backend

Watches the dependencies involved in resolution and plans when cached results
must be recomputed. It:

- Tracks dependencies between inputs, source nodes, strategies, and resolved values
- Watches for changes to those dependencies
- Determines which cached results are affected by a change
- Requests invalidation of affected cache entries
- Supports invalidation overrides that force regeneration even when a cached
	result would otherwise still be valid
- Records the planned invalidation and regeneration requirements for the
	Resolution Coordinator

The Planning Backend determines *when* a result must be regenerated. The Cache
Backend remains responsible for storing and removing the result.

## Audit Backend

Records the inputs, decisions, and outputs of the resolution process. It:

- Logs resolution events
- Logs strategy selection
- Logs LLM prompts
- Logs raw LLM outputs
- Logs post-processing
- Logs cache hits and misses
- Logs dependency and invalidation decisions
- Provides the history needed for replayability and auditability
- Records the information needed to assess determinism

The Audit Backend observes the resolution process; it does not compute
invalidation decisions or control cache contents.

## Resolution Backend / Resolution Coordinator

The Resolution Coordinator is the coordinating part of the Resolution Backend.
It is not a separate backend or an external frontend. The Resolution Backend
implements the backend-facing contract and delegates the work to its internal
components. It:

- Receives resolution requests
- Consults the Planning Backend to determine whether a cached result is valid
- Retrieves valid results from the Cache Backend
- Invokes the Strategy Backend when a result must be generated or regenerated
- Records resolution activity through the Audit Backend
- Stores newly resolved values in the Cache Backend
- Reports results and resolution status to the caller

The Resolution Backend owns the workflow at the system boundary, while its
internal components own resolution data, strategies, dependency planning, and
audit records respectively.