#pragma once
// Collective Fabric - portable topology model. Independent of any external
// topology library. Represents participants, nodes, devices, links, link
// classes, hierarchy, locality, nominal and measured capability, shared-path
// relationships, health, freshness, and provenance. UNKNOWN is a first-class
// value; synthetic topology never masquerades as measured.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/errors.hpp"
#include "collectivefabric/foundation/clock.hpp"
#include "collectivefabric/foundation/source.hpp"
#include "collectivefabric/digest/canonical.hpp"
#include "collectivefabric/digest/sha256.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <utility>

namespace collectivefabric {

struct LinkCapability {
  std::optional<std::uint64_t> bandwidth_bytes_per_sec;
  std::optional<std::uint64_t> latency_ns;
  bool is_known() const noexcept { return bandwidth_bytes_per_sec.has_value() || latency_ns.has_value(); }
  bool has_bandwidth() const noexcept { return bandwidth_bytes_per_sec.has_value(); }
};

struct TopologyLink {
  NodeId source_node;
  NodeId dest_node;
  DeviceId source_device;
  DeviceId dest_device;
  LinkClass link_class = LinkClass::UNKNOWN;
  LinkCapability nominal;
  LinkCapability measured;
  ProvenanceKind provenance = ProvenanceKind::UNKNOWN;
  HealthState health = HealthState::UNKNOWN;
  TopologyGeneration snapshot_generation;
  bool degraded = false;
};

struct NodeEntry {
  NodeId id;
  std::string host_name;
  std::vector<DeviceId> devices;
};

struct DeviceEntry {
  DeviceId id;
  NodeId node;
  std::string name;
  bool is_cuda = false;
  std::uint32_t compute_capability = 0;   // e.g. 120 == sm_120
  bool is_known = false;
};

class Topology {
public:
  explicit Topology(TopologyGeneration generation) : generation_(generation) {}

  const TopologyGeneration& generation() const noexcept { return generation_; }
  TopologyProvenance provenance() const noexcept { return provenance_; }
  void set_provenance(TopologyProvenance p) noexcept { provenance_ = p; }
  std::uint64_t created_utc_ns() const noexcept { return created_utc_ns_; }

  NodeId add_node(std::string host_name) {
    NodeId id(next_id_++);
    nodes_.push_back(NodeEntry{id, std::move(host_name), {}});
    return id;
  }
  DeviceId add_device(NodeId node, std::string name, bool is_cuda = false, std::uint32_t cc = 0) {
    if (!has_node(node)) throw Error(ErrorCode::VALIDATION, "add_device: unknown node");
    DeviceId id(next_id_++);
    devices_.push_back(DeviceEntry{id, node, std::move(name), is_cuda, cc, true});
    // associate device with node
    for (auto& n : nodes_) if (n.id == node) n.devices.push_back(id);
    return id;
  }
  void add_link(TopologyLink link) {
    if (!has_node(link.source_node) || !has_node(link.dest_node)) {
      throw Error(ErrorCode::VALIDATION, "add_link: unknown node");
    }
    if (!has_device(link.source_device) || !has_device(link.dest_device)) {
      throw Error(ErrorCode::VALIDATION, "add_link: unknown device");
    }
    link.snapshot_generation = generation_;
    links_.push_back(std::move(link));
  }

  std::size_t node_count() const noexcept { return nodes_.size(); }
  std::size_t device_count() const noexcept { return devices_.size(); }
  std::size_t link_count() const noexcept { return links_.size(); }
  const std::vector<NodeEntry>& nodes() const noexcept { return nodes_; }
  const std::vector<DeviceEntry>& devices() const noexcept { return devices_; }
  const std::vector<TopologyLink>& links() const noexcept { return links_; }

  bool has_node(NodeId n) const noexcept {
    return std::any_of(nodes_.begin(), nodes_.end(), [&](const NodeEntry& e) { return e.id == n; });
  }
  bool has_device(DeviceId d) const noexcept {
    return std::any_of(devices_.begin(), devices_.end(), [&](const DeviceEntry& e) { return e.id == d; });
  }

  std::optional<NodeId> node_of(DeviceId d) const noexcept {
    for (const auto& e : devices_) if (e.id == d) return e.node;
    return std::nullopt;
  }
  std::optional<DeviceId> device_with_capacity(NodeId node, bool cuda) const noexcept {
    for (const auto& e : devices_) if (e.node == node && e.is_cuda == cuda) return e.id;
    return std::nullopt;
  }

  // Best known link between two devices, preferring measured over nominal.
  std::optional<LinkCapability> link_between(DeviceId a, DeviceId b) const noexcept {
    for (const auto& l : links_) {
      if ((l.source_device == a && l.dest_device == b) || (l.source_device == b && l.dest_device == a)) {
        if (l.measured.is_known()) return l.measured;
        if (l.nominal.is_known()) return l.nominal;
        return LinkCapability{};
      }
    }
    return std::nullopt;
  }

  // Lowest link class (most local) connecting two devices, if any.
  std::optional<LinkClass> locality_between(DeviceId a, DeviceId b) const noexcept {
    auto na = node_of(a);
    auto nb = node_of(b);
    if (na && nb && *na == *nb) return LinkClass::INTRA_PROCESS;
    std::optional<LinkClass> best;
    for (const auto& l : links_) {
      if ((l.source_device == a && l.dest_device == b) || (l.source_device == b && l.dest_device == a)) {
        if (!best || link_class_rank(l.link_class) < link_class_rank(*best)) best = l.link_class;
      }
    }
    return best;
  }

  bool same_node(DeviceId a, DeviceId b) const noexcept {
    auto na = node_of(a); auto nb = node_of(b);
    return na && nb && *na == *nb;
  }

  // Count of device pairs sharing a common node (intra-node structure).
  std::size_t intra_node_device_pairs() const noexcept {
    std::size_t count = 0;
    for (const auto& n : nodes_) {
      std::size_t d = n.devices.size();
      count += d * (d - 1) / 2;
    }
    return count;
  }

  // Deterministic canonical digest of the topology snapshot.
  Sha256::Digest digest() const {
    CanonicalWriter w;
    w.u64(generation_.value());
    w.u8(static_cast<std::uint8_t>(provenance_));
    w.u64(created_utc_ns_);
    w.u64(nodes_.size());
    for (const auto& n : nodes_) {
      w.u64(n.id.raw());
      w.string(n.host_name);
    }
    w.u64(devices_.size());
    for (const auto& d : devices_) {
      w.u64(d.id.raw());
      w.u64(d.node.raw());
      w.string(d.name);
      w.boolean(d.is_cuda);
      w.u32(d.compute_capability);
      w.boolean(d.is_known);
    }
    w.u64(links_.size());
    for (const auto& l : links_) {
      w.u64(l.source_node.raw()); w.u64(l.dest_node.raw());
      w.u64(l.source_device.raw()); w.u64(l.dest_device.raw());
      w.u8(static_cast<std::uint8_t>(l.link_class));
      w.boolean(l.nominal.bandwidth_bytes_per_sec.has_value());
      if (l.nominal.bandwidth_bytes_per_sec) w.u64(*l.nominal.bandwidth_bytes_per_sec);
      w.boolean(l.nominal.latency_ns.has_value());
      if (l.nominal.latency_ns) w.u64(*l.nominal.latency_ns);
      w.boolean(l.measured.bandwidth_bytes_per_sec.has_value());
      if (l.measured.bandwidth_bytes_per_sec) w.u64(*l.measured.bandwidth_bytes_per_sec);
      w.boolean(l.measured.latency_ns.has_value());
      if (l.measured.latency_ns) w.u64(*l.measured.latency_ns);
      w.u8(static_cast<std::uint8_t>(l.provenance));
      w.u8(static_cast<std::uint8_t>(l.health));
      w.boolean(l.degraded);
    }
    return Sha256::digest(w.data());
  }

private:
  static int link_class_rank(LinkClass c) noexcept {
    switch (c) {
      case LinkClass::INTRA_PROCESS: return 0;
      case LinkClass::SHARED_MEMORY: return 1;
      case LinkClass::HOST_MEMORY: return 2;
      case LinkClass::NVLINK_CLASS: return 3;
      case LinkClass::RDMA_CLASS: return 4;
      case LinkClass::PCIE: return 5;
      case LinkClass::NETWORK: return 6;
      case LinkClass::UNKNOWN: return 7;
    }
    return 8;
  }

  TopologyGeneration generation_;
  TopologyProvenance provenance_ = TopologyProvenance::UNKNOWN;
  std::uint64_t created_utc_ns_ = clock::wall_ns();
  std::uint64_t next_id_ = 1;
  std::vector<NodeEntry> nodes_;
  std::vector<DeviceEntry> devices_;
  std::vector<TopologyLink> links_;
};

} // namespace collectivefabric
