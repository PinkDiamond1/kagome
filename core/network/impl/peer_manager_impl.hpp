/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_NETWORK_PEERMANAGERIMPL
#define KAGOME_NETWORK_PEERMANAGERIMPL

#include "network/peer_manager.hpp"

#include <memory>
#include <queue>

#include <libp2p/basic/scheduler.hpp>
#include <libp2p/event/bus.hpp>
#include <libp2p/host/host.hpp>
#include <libp2p/protocol/identify/identify.hpp>
#include <libp2p/protocol/kademlia/kademlia.hpp>

#include "application/app_configuration.hpp"
#include "application/app_state_manager.hpp"
#include "application/chain_spec.hpp"
#include "blockchain/block_storage.hpp"
#include "blockchain/block_tree.hpp"
#include "clock/clock.hpp"
#include "crypto/hasher.hpp"
#include "log/logger.hpp"
#include "network/impl/protocols/block_announce_protocol.hpp"
#include "network/impl/protocols/propagate_transactions_protocol.hpp"
#include "network/impl/protocols/protocol_factory.hpp"
#include "network/impl/stream_engine.hpp"
#include "network/protocols/sync_protocol.hpp"
#include "network/router.hpp"
#include "network/types/block_announce.hpp"
#include "network/types/bootstrap_nodes.hpp"
#include "network/types/own_peer_info.hpp"
#include "scale/libp2p_types.hpp"
#include "storage/buffer_map_types.hpp"

namespace kagome::network {

  class PeerManagerImpl : public PeerManager,
                          public std::enable_shared_from_this<PeerManagerImpl> {
   public:
    static constexpr std::chrono::seconds kTimeoutForConnecting{15};

    PeerManagerImpl(
        std::shared_ptr<application::AppStateManager> app_state_manager,
        libp2p::Host &host,
        std::shared_ptr<libp2p::protocol::Identify> identify,
        std::shared_ptr<libp2p::protocol::kademlia::Kademlia> kademlia,
        std::shared_ptr<libp2p::basic::Scheduler> scheduler,
        std::shared_ptr<StreamEngine> stream_engine,
        const application::AppConfiguration &app_config,
        std::shared_ptr<clock::SteadyClock> clock,
        const BootstrapNodes &bootstrap_nodes,
        const OwnPeerInfo &own_peer_info,
        std::shared_ptr<network::Router> router,
        std::shared_ptr<storage::BufferStorage> storage);

    /** @see AppStateManager::takeControl */
    bool prepare();

    /** @see AppStateManager::takeControl */
    bool start();

    /** @see AppStateManager::takeControl */
    void stop();

    /** @see PeerManager::connectToPeer */
    void connectToPeer(const PeerInfo &peer_info) override;

    /** @see PeerManager::reserveStreams */
    void reserveStreams(const PeerId &peer_id) const override;

    /** @see PeerManager::activePeersNumber */
    size_t activePeersNumber() const override;

    /** @see PeerManager::forEachPeer */
    void forEachPeer(std::function<void(const PeerId &)> func) const override;

    /** @see PeerManager::forOnePeer */
    void forOnePeer(const PeerId &peer_id,
                    std::function<void(const PeerId &)> func) const override;

    /** @see PeerManager::forOnePeer */
    void keepAlive(const PeerId &peer_id) override;

    /** @see PeerManager::startPingingPeer */
    void startPingingPeer(const PeerId &peer_id) override;

    /** @see PeerManager::updatePeerStatus */
    void updatePeerStatus(const PeerId &peer_id, const Status &status) override;

    /** @see PeerManager::updatePeerStatus */
    void updatePeerStatus(const PeerId &peer_id,
                          const BlockInfo &best_block) override;

    /** @see PeerManager::getStatus */
    boost::optional<Status> getPeerStatus(const PeerId &peer_id) override;

   private:
    /// Right way to check self peer as it takes into account dev mode
    bool isSelfPeer(const PeerId &peer_id) const;

    /// Aligns amount of connected streams
    void align();

    void processDiscoveredPeer(const PeerId &peer_id);

    void processFullyConnectedPeer(const PeerId &peer_id);

    /// Opens streams set for special peer (i.e. new-discovered)
    void connectToPeer(const PeerId &peer_id);

    /// Closes all streams of provided peer
    void disconnectFromPeer(const PeerId &peer_id);

    std::vector<scale::PeerInfoSerializable> loadLastActivePeers();

    /// Stores peers addresses to the state storage to warm up the following
    /// node start
    void storeActivePeers();

    std::shared_ptr<application::AppStateManager> app_state_manager_;
    libp2p::Host &host_;
    std::shared_ptr<libp2p::protocol::Identify> identify_;
    std::shared_ptr<libp2p::protocol::kademlia::Kademlia> kademlia_;
    std::shared_ptr<libp2p::basic::Scheduler> scheduler_;
    std::shared_ptr<StreamEngine> stream_engine_;
    const application::AppConfiguration &app_config_;
    std::shared_ptr<clock::SteadyClock> clock_;
    const BootstrapNodes &bootstrap_nodes_;
    const OwnPeerInfo &own_peer_info_;
    std::shared_ptr<network::Router> router_;
    std::shared_ptr<storage::BufferStorage> storage_;

    libp2p::event::Handle add_peer_handle_;
    std::unordered_set<PeerId> peers_in_queue_;
    std::deque<std::reference_wrapper<const PeerId>> queue_to_connect_;
    std::unordered_set<PeerId> connecting_peers_;
    std::unordered_set<libp2p::network::ConnectionManager::ConnectionSPtr>
        pinging_connections_;

    struct ActivePeerData {
      clock::SteadyClock::TimePoint time;
      Status status{};
    };

    std::map<PeerId, ActivePeerData> active_peers_;
    libp2p::basic::Scheduler::Handle align_timer_;
    std::set<PeerId> recently_active_peers_;

    log::Logger log_;
  };

}  // namespace kagome::network

#endif  // KAGOME_NETWORK_PEERMANAGERIMPL
