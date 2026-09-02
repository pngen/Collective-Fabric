#include "test_util.hpp"
#include "collectivefabric/topology.hpp"
using namespace collectivefabric;

int main() {
  Topology t(TopologyGeneration(1));
  t.set_provenance(TopologyProvenance::SYNTHETIC);
  auto n1 = t.add_node("n1");
  auto n2 = t.add_node("n2");
  auto a = t.add_device(n1, "a", false, 0);
  auto b = t.add_device(n1, "b", false, 0);
  auto c = t.add_device(n2, "c", true, 120);
  auto d = t.add_device(n2, "d", true, 120);

  TopologyLink l1; l1.source_node=n1; l1.dest_node=n1; l1.source_device=a; l1.dest_device=b;
  l1.link_class = LinkClass::SHARED_MEMORY; l1.provenance = ProvenanceKind::SYNTHETIC;
  t.add_link(l1);
  TopologyLink l2; l2.source_node=n1; l2.dest_node=n2; l2.source_device=b; l2.dest_device=c;
  l2.link_class = LinkClass::NETWORK; l2.provenance = ProvenanceKind::SYNTHETIC;
  t.add_link(l2);

  CF_CHECK(t.node_count() == 2 && t.device_count() == 4 && t.link_count() == 2);
  CF_CHECK(t.same_node(a, b));
  CF_CHECK(!t.same_node(a, c));
  CF_CHECK(t.node_of(c) == n2);
  CF_CHECK(t.locality_between(a, b) == LinkClass::INTRA_PROCESS);
  CF_CHECK(t.locality_between(c, d) == LinkClass::INTRA_PROCESS);  // same node, no link
  CF_CHECK(t.intra_node_device_pairs() == 2);  // each node has 2 devices -> 1 pair each

  // link_between: unknown connectivity -> nullopt
  CF_CHECK(!t.link_between(a, c).has_value());
  // digest stable
  auto dg1 = t.digest();
  auto dg2 = t.digest();
  CF_CHECK(dg1 == dg2);

  // add_device on unknown node rejected
  CF_CHECK_THROWS(t.add_device(NodeId(999), "x", true, 0));

  // digest changes with topology generation / content
  Topology t2(TopologyGeneration(2));
  t2.set_provenance(TopologyProvenance::SYNTHETIC);
  auto m = t2.add_node("x");
  auto da = t2.add_device(m, "a", true, 120);
  CF_CHECK(t2.digest() != t.digest());

  CF_FINISH("test_topology");
}
