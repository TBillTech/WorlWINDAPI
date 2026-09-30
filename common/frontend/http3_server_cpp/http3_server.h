#pragma once

#include "backend.h"
#include "http3_tree_message.h"
#include "frontend_base.h"
#include "communication.h"

// This HTTP3Server class does not need to perform authentication or authorization, because that job will be handled by the
// HTTP3Authenticator service, which stands between clients and these HTTP3Server instances.  The job of the HTTP3Authenticator
// is to block scid creation by unauthorized clients, and to filter static asset request, various tree reads, 
// various tree writes (separate level of authority), and to filter the returning journal notifications.
// This does imply one thing about journal notifications: the response can contain just the sequence number,
// hiding the notification itself, but keeping the client up-to-date without revealing secrets.

// The HTTP3ServerRoute class is a utility class which operates inside the Server Handler callback in the QuicListener.
// It can also provides static "assets" for urls which do not require the callback listener.
// If an url is NOT a chunk stream, then the assumption is that the url is a static asset and can be immediately served.
// On the other hand, the journaling and backend api support will be chunk stream mode.
// In fact, by default, the distinction for is_chunk_stream just checks if the url contains the path /wwatp 
// WWATP tree protocol then functions beyond the /wwatp path point, and multiple different sets of journals can 
// function independently of each other.  The HTTP3ServerRoute class is thus more of a path resource provider than a standalone 
// server executable process.  (In contrast, the HTTP3Server is close to the core of an executable process.)
class Http3ServerRoute {
public:
    Http3ServerRoute(Backend& backend, size_t max_journal_size, 
                     std::string const& url, std::string const& mutable_page_label_rule) 
        : backend_(backend), lastNotificationIndex_(0), maxJournalSize_(max_journal_size),
          url_(url), mutablePageLabelRule_(mutable_page_label_rule) {
            journal_.push_back({0, {"", fplus::nothing<TreeNode>()}});
          };
    ~Http3ServerRoute() = default;

    Http3ServerRoute(const Http3ServerRoute&& other) :
        backend_(other.backend_), lastNotificationIndex_(other.lastNotificationIndex_),
        journal_(std::move(other.journal_)), maxJournalSize_(other.maxJournalSize_),
        url_(std::move(other.url_)), mutablePageLabelRule_(std::move(other.mutablePageLabelRule_)) {
        };

    // A some things of note:
    // * For the purpose of keeping Journaling sane, all clients are assumed to recieve the same notifications. 
    //   If some clients really want to have very different notification sets, then they really should connect to different routes.
    //   Since multiple server routes can be connected to the same backend (composite/redirected), this is not a problem.
    // * Whenever a client registers as a listener, the server will add the listener for all clients, and also track the total
    //   set of listening labels.  This obviates the need to track callbacks separately for each client.
    // * We will try the following algorithm for server reset:
    //   * A WRONG_JOURNAL_SEQUENCE is a SequentialNotification with number 0, then the number 2, then the latest sequence number.
    //   * If the client requests a notification sequence number > than the server last notification, then the server will send WRONG_JOURNAL_SEQUENCE
    //   * If the client requests a notification sequence number < than the server first notification, then the server will send WRONG_JOURNAL_SEQUENCE
    //   * Thus, the the worst case scenario seems to be if the server restarts, and the sequence number grows past the client's last notification 
    //     before the client can request the next notification.  To fail, the client would have to wait to request a notification more seconds than
    //     the sequence number / notifications per second.  If the server has been running a long time, then this is unlikely to happen. It is more
    //     of a problem when the service is bouncing a lot.
    // * The server can also add in unsolicited notifications without any sequence number (uint64_t)-1.  This is to support changes the client 
    //   tried to make, but the server rejected.  The unsolicited notifications will be undoing the attempted changes.
    chunks processResponseStream(const StreamIdentifier& stream_id, chunks& request);

    void buildResponseChunks(HTTP3TreeMessage& requested);

    HTTP3TreeMessage intializeResponseMessage(const StreamIdentifier& stream_id, chunks& request) const;

    Backend& getBackend() const { return backend_; }

private:
    Backend& backend_;
    std::map<StreamIdentifier, HTTP3TreeMessage> ongoingResponses_;
    // Journal is an ordered list of notifications. 
    // The concept here is that if the server cannot provde the all the notifications back to the client,
    // then the client will have to "start over" and request the mutable part of the tree again.
    // Of course, the client is allowed to request static parts of the tree as well.
    uint64_t lastNotificationIndex_;
    std::deque<SequentialNotification> journal_;
    std::map<std::string, bool> listeningLabels_;
    size_t maxJournalSize_;
    std::string url_;
    std::string mutablePageLabelRule_;
};

class HTTP3Server : public Frontend {
public:
    HTTP3Server(const std::string& name) : name_(name) { };
    HTTP3Server(const std::string& name, std::map<std::string, chunks>&& staticAssets) 
        : name_(name), staticAssets_(std::move(staticAssets)) { };
    ~HTTP3Server() = default;

    // Frontend interface implementation
    std::string getName() const override { return name_; }
    std::string getType() const override { return "http3_server"; }
    
    void stop() override {
        stopFlag.store(true);
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }

    bool isRunning() const override {
        return !stopFlag.load();
    }

    std::vector<Backend*> getBackends() override {
        std::vector<Backend*> backends;
        for (const auto& [url, route] : servers_) {
            backends.push_back(&route.getBackend());
        }
        return backends;
    }

    Http3ServerRoute& addBackendRoute(Backend& backend, size_t journal_size, std::string const& url) {
        auto emplaced = servers_.emplace(url, Http3ServerRoute(backend, journal_size, url, "MutableNodes"));
        return emplaced.first->second;
    }

    void addStaticAsset(std::string const& url, chunks& asset) {
        staticAssets_.emplace(url, asset);
    }

    bool isChunkStream(std::string const& url) const;
    // The static asset method servers static assets from the server, and also has special case logic for /index.html (and /),
    // and will ususally compose it from the backend index node page. It will also usually inject the css and js from the tree into the page.
    chunks staticAsset(std::string const& url) const;

    prepared_stream_callback getResponseCallback(const Request & req);

    // It is expected that the main thread will use this class in one of two modes:
    // 1. In an explicit work mode, which feathers maintaining the handlers with
    //    the processResponseStream method in some fashion, such as:
    //    while (true) {
    //        connector.processResponseStream();
    //        sleep_for(std::chrono::milliseconds(100));
    //    }
    // 2. As a daemon thread, using the below function, which will create new thread 
    //    and do the above work until the stop flag is set to true.
    void start(Communication& connector, double, size_t sleep_milli = 100) override {
        stopFlag.store(false);
        serverThread_ = thread([this, &connector, sleep_milli]() {
            while (!stopFlag.load()) {
                connector.processResponseStream();
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_milli));
            }
            return EXIT_SUCCESS;
        });
    }


private:
    std::string name_;
    std::map<std::string, Http3ServerRoute> servers_;
    std::map<std::string, chunks> staticAssets_;
    std::map<StreamIdentifier, std::string> requestRoutes_;

    thread serverThread_; // Thread to run the server
    std::atomic<bool> stopFlag{false}; // Flag to stop the server thread
};