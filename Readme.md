# World Wide Introspective Namespace Dendron Protocol

The World Wide Introspective Namespace Dendron (WorlWIND) Protocol is an agent communication layer on top of HTTP3 aware sockets (or plain socket). Each end-point conceptually contains a "growing" tree. In general, trees can grow in several ways:
- New branches and/or leaves.
- Pruned branches and/or leaves.
- Mutations of branches and/or leaves.

What makes this an _namespace_ dendron protocol is that the description node references can be personality prompts to agents templated by the node label rule! And because queries can be agent-like string requests prompted by the Q&A of a node. Another way to look at this is to say that the literal API of clients and servers of the tree protocol is infrastructure only: the content API is implicit and based around LLM friendly two way discussions about capabilities and content descriptions.

## Build and test

Use a fresh build directory per platform/toolchain (do not reuse a Linux build directory for MinGW, or vice versa).

### Linux

```bash
cmake -S . -B build-linux -G "Unix Makefiles"
cmake --build build-linux --parallel
ctest --test-dir build-linux --output-on-failure
```

### MinGW (msys64, native)

Use the **clang64** MSYS2 environment (`mingw-w64-clang-x86_64-toolchain`), not `mingw64`/GCC: the GCC packaged in `mingw64` (a GCC 16 dev snapshot at the time of writing) has a non-deterministic internal compiler error parsing AVX-512 intrinsic headers whenever `<windows.h>` is included, with no stable flag workaround across all vendored sources. Clang does not hit this bug.

From an **MSYS2 CLANG64** shell (or the `MSYS2 CLANG64` VS Code terminal profile in `.vscode/settings.json`):

```bash
cmake -S . -B build-clang64 -G "MinGW Makefiles" \
  -DCMAKE_C_COMPILER=/clang64/bin/clang \
  -DCMAKE_CXX_COMPILER=/clang64/bin/clang++ \
  -DBUILD_WWATP_QUIC_C=OFF
cmake --build build-clang64 --parallel
ctest --test-dir build-clang64 --output-on-failure
```

(Equivalently: `cmake --preset clang64` using the repo's `CMakePresets.json`.)

Prerequisites (install via `pacman` in the clang64 shell): `mingw-w64-clang-x86_64-toolchain`, `mingw-w64-clang-x86_64-cmake`, `autoconf`, `automake`, `libtool`. **`libev` is not packaged for MinGW/clang64 in MSYS2** (only `libevent`, a different API) and must be built from source (`http://dist.schmorp.de/libev/`) with `./configure --prefix=/clang64 --host=x86_64-w64-mingw32 && make && make install`.

Notes:
- `--disable-dependency-tracking` is required because clang64's `mingw32-make` doesn't set `$(MAKE)` the way these autotools scripts expect.
- Avoid `make -j`: parallel jobs were observed to hang under this MSYS2 `make`/libtool combination; build serially.

## Client and Server instances and ecosystem

There are multiple software instances attached to the tree protocol with the following typical uses:
- **Server**: A Server instance attached to the tree protocol which efficiently stores and maintains the Tree data, and connects to multiple authentication and server peer nodes. It _may_ also communicate using inter process communication to increase bandwidth if a trusted client is running on the same CPU.  It usually has a relational database back end.
- **Authentication Node**: An instance whose job is to keep a version of the tree which exactly matches the authorization level of this node. No more, no less. This node has these sub trees:
  - Authorization branch: Not visible or queryable by clients.  Contains all the information necessary for this node to behave correctly.
  - Public branch: visible and queryable by all clients.
  - Private branch: only visible and queryable by the exact client.
- **Load Balancer Node**: From an IP standpoint, actually is the service at the IP address and port of the tree.  It's job is to redirect traffic flows, and so it possesses all the Authorization branches (and keeps nothing else) and knows which authorization instances are available to match and service them. Also has legacy tunneling from Client facing http2 websockets down to Authentication node facing QUIC connections.
- **Client**: Conceptually, connects directly to a server (usually in fact a load balancer instance), and is linked into the client software as a library. Implementations must allow the server to provide proxy sources transparently, and must allow the linked software to control how much of the tree is kept and maintained in memory. There is a javascript web browser implementation library that uses legacy http3 websockets to connect to the load balancer node.

## Tree Structure

The tree protocol has a tree structure, with node branches having any number of node children. A leaf is simply a node with no children. A branch is simply a node with children. A node has:
- **A _unique_ global label rule**: UTF8 encoded string
- **A description**: UTF8 encoded string OR a node reference
- **Optional How to query**: node reference to description of how to query and unpack nodes matching the label rule (If absent, means just the point query)
- **Optional Q&A sequence**: node reference to Question and Answer session about the node
- **A literal type**: UTF8 encoded type name, or list of type names (see below)
- **Optional policy**: node reference to versioning policy
- **A version**:
  - Sequence Number % Max Sequence, Max Sequence, UTF8 policy
  - Optional: Authorial proof (a node reference)
  - Optional: Authors: for TokenRing A node containing the authors, permissions, rules, etc.
  - Optional: Readers
  - Optional: Collision depth
- **Child names**: List of [UTF8 label templates]
- **Contents**: Length-Value bytes

## Lineage Representation

A common idiom is to represent lineage by the concatenation of labels by inserting '/' characters, and prepending with `wwatp://<addr>:<port>/` or `udp://<addr>:<port>/`, where `<addr>` is an IP address and `<port>` is the port number. Notionally, there should NOT be more than one tree protocol at any addr+port destination, because that just means there should be two sub-trees connected at the root.

## UTF8 Encoded Type Names

The UTF8 encoded type name is one of the JSON types, with the addition of Bytes and Callbacks:
- UTF8 String
- Int
- Float
- Image
- List
- Dictionary
- Bytes
- Datetime (UTF8 string coding like "DD-MM-YYYY HH:MM:SS.SSSS UTC")
- Callback
- Removed - means the node and all its children have been dropped from the tree.
- Error - means that a problem was encountered reading the stream, and the node was not correctly decoded.

Normally, branches will be labelled with List, Dictionary, or Callback, while leaves will be labelled UTF8 String, Int, Float, Image.

## Tree Encoding

The literal encoding of the full tree at the UDP layer is as a page of the total depth-first list of `<Length-Value>` node representations, for example:
{<Node uuid><Node checksum>}<Node label><Node description><Node type><Node version><Node child names><Node contents>
The Node uuid and checksum are present for udp implementations, and absent for tcp implementations. This allows for udp implementations to detect errors in the stream.

## Version Conflict Policies

Implementations should honor several common version conflict policies:
- **Reader-Writer policy**: There is one owner/reader of any given node. Only the owner is allowed to modify that node (matching the author ID), and any version of the node that has an author other than the owner is immediately rejected. Nodes without Author IDs are owned by parents. If authorial proof is provided, implementations are required to check the consistency of the signature and reject false signatures.
- **Token Ring policy**: Like the writer part of the reader-writer policy, except after each new version of the node(s), only the next Author ID in the ring has write permission. New versions that use the wrong author ID are rejected. The author ring itself is a list child of the node with reserved name "tokenring", and children author names with maybe child Bytes with label "certificate". If no changes are made by an agent, then that agent should set the Author ID to itself, and leave the sequence number unchanged. The author ring also has a properties "timeout", "count" and "votes". Votes are reader-writer owned by the authors in the ring, and when the count vote has been cast, after timeout time the token will be implicitly moved to the next author in the ring. This is to prevent dead-lock.
- **Collision Rollback Policy**: All authors can freely read/write any node. Collision depth versions of nodes and all children must be kept as history until a version is final. If two messages with the same sequence ID are seen, then all agents will roll back the node to the sequence number one prior, and all "future" history will be deleted. The snapshot will have the children states at the time the rollback sequence number was first seen. Implementations should wait a random amount of time before sending the conflicting sequence change, increasing this wait time as collisions continue to occur (similar to how ethernet works).

It is recommended that only changes to the tree be sent across the protocol unless an agent specifically requests the entire tree. By definition, every callback is a pending tree substitution, and note that the Callback value has the following possible values:
- Once
- Auto

## Underlying technologies for transport layer

At present the Agents attaching to the tree will use QUIC and HTTP/3 communication.  This will permit, for example, different branches of the tree to be sent either whole or in part to multiple other agents simultaneously.

For now, the QUIC + HTTP/3 library will be nghttp3.

After further reasearch, the HTTP/3 protocol shows some difficulties. Crucially, it appears that when the server blocks due to no data ready, then the client also blocks.  This implies that the desired following flow is broken.  The client should connect to the server on a stream, then client sends messages to the server, then the server reads those messages, then the server replies to the messages, and repeat. The broken bit is that when the server "blocks" based on not having more data to send, then the client also "blocks" on sending data even if it has data to send.

Workaround: Logically disconnect RequestIdentifiers of the QuicConnector and QuicListener from QUIC streams. On client side, send a "request" which is actually a sequence of chunks with RequestIdentifiers in a batch.  Then, on server side, the new "stream" gets created, with a single "request" composed of a batch of RequestIdentified chunks, and then a single "reply" composed of a batch of RequestIdentified result chunks is sent back allowing the stream to close.  If there is enough data being sent back and forth, it might even keep the stream open.


## Backends, Frontends, and how they are composed

While the transport layer is super important, it is also abstracted away.  A frontend choice combined with a backend choice in effect creates an application.  Every frontend has exactly one backend.  Backends, on the other hand, can be composed of multiple other backends, or they may virtualize parts of the tree via agentic generation.

A special remark for supporting web applications is that http3_server_cpp has wwatp behavior only for the magic wwatp tree root.  Otherwise, index.html will provide the dom bootstrap logic which can usually make "something" out of the tree in terms of css and dom elements.  I'm expecting to create an LLM tool called a css-watcher that watches both the css file and the tree, synchronizing them, and providing the css as a node in the tree that the index.html will reference. Then mostly what is required by the js_client_lib is to apply the dom_link_javascript frontend.

Current architecture has the following details:
cpp_client_lib supports developing a tool (tools can be applications with UIs):
* frontend is direct_cpp for a tool
* backend is http3_client for a tool
js_client_lib supports developing a web application:
* frontend is direct_javascript for a web application
* backend is http3_client for a web application
The server program generally supports simple services without additional development, including services that need to restructure or remap the tree in simple ways, and services which can be backed by trees in memory (and on disk):
* frontend is http3_server_cpp for a service
* backend is http3_client, memory, or composite (which composes more composite, http3_client, and memory backends)
cpp_client_lib also supports developing custom services (for example, a custom postgres service)
* frontend is http3_server_cpp for a custom service
* backend implements interface
* If using the server program for all composition needs is too burdensome on system resources, a custom service can also use composite and other backends

To summarize what goes into the two libraries, everything from common:
cpp_client_lib: frontend; direct_cpp http3_server_cpp, backend_cpp; composite interface http3_client memory 
js_client_lib: frontend; direct_javascript dom_link_javascript, backend_js; http3_client

Here are some examples:
* A game client composed of an unreal_engine frontend using an http3_client backend_cpp.
* A game server composed of an http3_server_cpp frontend and a postgres backend_cpp.
* A digital employee composed of:
    * A browser dom_link_javascript frontend using an http3_client backend_js.
    * A composer for browser with http3_server_cpp frontend presenting the curent task, backlog, status, and the css blob (memory backend?)
    * A css watcher tool with css_watcher frontend and http3_client backend_js informing the composer.
    * A wwatp hub http3_server_cpp frontend using a postgres backend_cpp.
    * A worker manager using direct_cpp frontend and http3_client backend_cpp, which can create worker/tool processes:
    * Multiple worker/tool processes using direct_cpp frontend and http3_client backend_cpp, additionally tools having various ways to transform tree information to additional tree information.  For example, performing algebra on equations using maxima.  Or watching a yaml file, and keeping the tree in sync with the yaml file.

The following backends are tested in the project:
* Simple Backend: Simply delegates tree data storage to a memory tree object, and supports applying transactions.  This implementation can stand alone, unlike most of the others.
* Transactional Backend: Supportings building a transaction in memory, and then attempting to apply to the underlying backend.
* Threadsafe Backend: Controls access to the underlying backend, so that the threadsafe backend can be used by multiple backends all used by different threads. It supports worker threads dealing with notifications to avoid blocking other notifications.
* Composite & Redirection Backends: Will allow mounting branches from other Backends as branches in a composed greater tree.

Currently, the following backends are planned.  Note these usually assume an underlying backend, not a direct implementation:
* NOT GOING TO IMPLEMENT: JournalingBackend: because as far as I know only the http3_client needs this, and only the http3_server_cpp supports this.  So rather than build journaling into the most general backend interface, the journaling concept will be implemented along with http3 and the usage of the transport layer.
* PostGRES Backend: Will also be a standalone implementation, and will present a PostGRES database as a Tree using the Backend interface.  But this will be developed in a separate project, not in WorlWIND project.
* HTTP3Client Backend: Will keep a current cached tree, and track changes to it via journaling, and connect to a http3_server_cpp instance which can support querying the journal, and fast fowarding changes.

Terminology for frontend capabilities:
* A Watcher has a Context and waits for a change on the input, and then regenerates the output (for example, a file, or another tree).
* A Mediator is composed of two Watchers, with each of two Contexts, which are inverses of each other.
* An Application is composed of a Watcher for the Observable Tree input <-> View output, and multiple Watchers for Controller input <-> Callback Tree output
* A Tool is composed of a Watcher for the Observable Tree input <-> Controller output, and multiple Watchers for Controller input <-> Callback Tree output
* A Service is composed of a Watcher for the Backend Tree input <-> Transported Tree output, and a Watcher for Transported Tree input <-> Backend Tree output

The following frontends are tested in the project:


Currently, the following frontends are planned.  Most of these will not be done in the WWATP project, and these are indicated by (WWATP):
* HTTP3Application Frontend Service (WWATP): Will support querying the journal and fast forward in detail.  If the URL is not a tree query, will return a web page bootstrap which will run the DOMLinkFrontend.  The server frontend will provide CSS from the tree, and scripting from the tree, supporting a LLMToolFrontend-> generated CSS and LLMToolFrontend -> generated javascript.
* DOMLink Frontend Application (WWATP): Connects to the javscript HTTP3ClientBackend, and maps tree nodes to the DOM with identifiers, callbacks, and classes from the CSS.  Probably will try to use a concept of DOM templating to expand a node.
* YAML Frontend Mediator (WWATP): Simultaneously watches a tree and a YAML node, and keeps them in sync. (no LLM needed)
* Flat HTML Frontend Mediator (WWATP): Simultaneously watches a tree and a flat (but dynamically scriptable) HTML node, and keeps them in sync.
* Static HTML Frontend Watcher (WWATP): Simply outputs a static flat HTML node (using PNGs for images), suitable for postprocessing to EPUB.
* Flat SSML Frontend Mediator (WWATP): Simultaneously watches a tree and a SSML node, and keeps them in sync (usable with the Merge Tool). 
* File Frontend Wacher (WWATP): Simultaneously watches a tree and a file, and keeps them in sync.
* Careful Prose Tool Frontend Mediator: Simultaneously watches a tree and a text node, and keeps them in sync.  Functionality can vary widely based on LLM prompting and tree context used. The "Careful" descriptor means that the mapping from LLM output to the tree is validated each direction.  A tree change requires the LLM to re-construct the tree from the Prose for the prose to be considered correct.  A change in the Prose mapping to a change in the tree cannot be considered correct unless the LLM can re-construct the prior version of the tree from the current version of the tree and the current and prior versions of the Prose.  This tool may also invoke validation Tools (either raw Merge Tool or via a Tool Map Tool) to verify the result passes a schema.
* Merge Tool Frontend Mediator (WWATP): Simultaneously watches two trees, and synchronizes the intersection of two trees versus a rule tree.  Option to freeze/lock one of the trees.
* Tool Map Tool Frontend Mediator: A Careful Prose Tool mapping a description of a Downstream Tool usage with valid input examples and invalid input examples outputting a Merge Tool rule tree. Whenever the Tool is applied to an input, if ever there is an error coming from the Downstream Tool, this will be added as a invalid input example rippling through to an update to the Merge Tool rules. For successful Downstream Tool uses, succesful examples will be added to the Prose unless it changes the Merge Tool rules.  New examples or counter examples may be suppressed if they are sufficiently like other examples or counter examples. 
* LaTeX Frontend Mediator: Not sure.  Maybe this can work like an HTTP3ServerFrontend with respect to CSS, which LaTeX sort of has a parallel concept.  This will use MathLive for LaTeX front <-> MathJSON back, LaTeX Chart front <-> declarative Chart back.
* Unreal Engine Frontend Application: Assets, levels, scripts, all generated by LLMs and placed into the tree. This should be a very similiar concept to the DOMLink Frontend.
* Maxima Tool Frontend Watcher: Watches mathJSON-like node complexes that evoke evaluations, and posts evaluated results back to the tree.
* Python Sandbox Frontend Watcher: Watches python-like (sandbox restricted) node complexes that evoke evaluations, and posts evaluated results back to the tree.
* Simple Testable Model Tool Frontend Watcher: Is a Tool Map Tool specification plus test example inputs and outputs, such that the Tool Map Tool validation cannot succeed unless the example inputs still correctly create the example outputs.
* Composed Model Tool Frontend Watcher: Is a Careful Prose Tool composed with child Careful Prose Tools describing Simple Testable Models in a useful sequence. The top level Careful Prose Tool describes the overall process of computation, prompted by which tools are available, what they can do, the overall inputs and outputs by label and schema, and the process broken down into numbered steps.  Note that the tree must specify step jump options so that the step transitions can be fully enumerated.  Each enumerated step transition corresponds to zero or more conditions and one or more transitions.  Each condition and transition is a Careful Prose Tool describing a Merge Tool mapping data to the input of a Simple Testable Model.  

MathJSON is defined elsewhere, but declarative Chart "representation" is defined as follows: All data points are defined as ordered lists, and all titles, legends, and and axis labels are MathJSON.  Based on this, declarative Plotly front is isomorphic to declarative Chart "representation", only with LaTeX labelling and added chart java script.  LaTeX Chart "representation" is the same chart in LaTeX format.

Nominally, displayable Trees contain several important content values, which can be discussed by agents. If a best noun and/or best verb changes from one context to another, then it is necessary to create a Mediator for the two contexts:
* label_rule: Required for all nodes, not atually in the content values, the node label (isomorphic to DOM id, the proper name/unique identifier) 
* IsATag: For this node, the IsATag is a field specifying what the object is (isomorphic to an HTML tag, the best noun)
* UseClass: For this node, the UseClass is a field specifying what use case the object supports (isomorphic to an HTML class, the best verb)
* attributes: For this node, any attributes as key value pairs (isomorphic to DOM attributes, direct object adjectives)

For use case writing a book and writing a paper, we want the following features.  We want to be able to edit scenes and chapter context (or anything really) directly with a straight YAML representation of the tree.  We want the ability to edit equations in the web browser.  We want the web browser to be able to display charts.  We want to be able to export it all to a Static HTML for EPUB.  We want to be able to have an LLM convert the structured data to prose.  We want the LLM to convert the structured data to SSML. To acheive these goals, the following frontend and backends will be needed:
* Threadsafe Backend at the DAG root, using Simple Backend with read and write.
* Composite & Redirection Backends, for restricting the Static HTML Domain
* HTTP3_Client Backend in cpp
* HTTP3_Server Frontend, for supporting HTTP3_Client cpp, HTTP3_Client javascript, Flat HTML Mediator, Static HTML Watcher, Flat SSML Mediator, DOMLink Frontend
* HTTP3_Client Backend in javascript, which is the ONLY javascript backend 
* Cloning Mediator Frontend + (if lazy flag, use earlier version notification in one tree to cause update)
* File Backend, for supporting writing out YAML and HTML files, which is isomorphic to directories with a node.txt and contents 
* YAML Mediator Frontend (WWATP Trees implicitly use MathJSON "representation", declarative Chart "representation")
* Flat HTML Mediator Frontend, using MathLive for LaTeX front <-> MathJSON back, PlotLY for declarative PlotLy front <- declarative Chart back
* Static HTML Watcher Frontend, using MathLive for HTML front <- LaTeX <- MathJSON back to LaTeX to HTML, PlotLY for PNG <- declarative PlotLy front <- declarative Chart back, and can use pandoc to convert to EPUB
* Flat SSML Mediator Frontend, which simply converts DOM elements into SSML markdown
* DOMLink Frontend, which also uses MathLive LaTeX DOM, implying HTTP3ServerFrontend should convert LaTeX front <-> MathJSON back, PlotLY for declarative PlotLy front <- declarative Chart back
* In a different project: Careful Prose Tool Mediator Frontend
* LaTeX Mediator Frontend, using MathLive for LaTeX front <-> MathJSON back, ? front <- declarative Chart back

There should be a lot of overlap between the Flat HTML Mediator (javacript? on node.js?) and the DOMLink, especially when it comes to editing formulas.  MathLive is probably the right library to use in the DOMLink: https://github.com/arnog/mathlive .  PlotLY seems to be the Chart library of choice.

Here are some additional front and backends that may eventually be useful:
* c FFI frontend, providing a variety of other FFI language backends using it.
* Unreal Engine Frontend
* Maxima Mediator Frontend
* Caching Backend, to speed up using File Backends and complicated queryable backends

## Third party libraries used

* https://www.editgym.com/fplus-api-search/
* nghttp3 + ngtcp2
