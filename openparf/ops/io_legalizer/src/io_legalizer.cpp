/**
 * File              : io_legalizer.cpp
 * Author            : Jing Mai <jingmai@pku.edu.cn>
 * Date              : 08.26.2021
 * Last Modified Date: 08.26.2021
 * Last Modified By  : Jing Mai <jingmai@pku.edu.cn>
 */
#include "io_legalizer.h"

#include <lemon/list_graph.h>
#include <lemon/network_simplex.h>

#include <algorithm>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

OPENPARF_BEGIN_NAMESPACE

using Site       = database::Site;
using Resource   = database::Resource;

using SiteRefVec = std::vector<std::reference_wrapper<const Site>>;

int32_t SelectResource(database::PlaceDB const &placedb, int32_t area_type_id) {
  int32_t num_resources   = placedb.numResources();
  int32_t selected_rsc_id = -1;
  for (int32_t rsc_id = 0; rsc_id < num_resources; rsc_id++) {
    const auto &area_type_ids = placedb.resourceAreaTypes(Resource(rsc_id));
    bool is_contained = std::find(area_type_ids.begin(), area_type_ids.end(), area_type_id) != area_type_ids.end();
    if (is_contained) {
      openparfAssert(selected_rsc_id == -1);
      selected_rsc_id = rsc_id;
    }
  }
  return selected_rsc_id;
}

SiteRefVec CollectBoxes(database::PlaceDB const &placedb, int32_t rsc_id) {
  auto const &layout = placedb.db()->layout();
  SiteRefVec  concerned_sites;
  for (const Site &site : layout.siteMap()) {
    auto const &site_type = layout.siteType(site);
    if (site_type.resourceCapacity(rsc_id) > 0) {
      concerned_sites.emplace_back(site);
    }
  }
  return std::move(concerned_sites);
}

template<class T, class V>
class BucketCounter {
 public:
  BucketCounter() : num_buckets_(0), size_(0) {}

  explicit BucketCounter(T num_buckets)
      : num_buckets_(num_buckets),
        size_(0),
        buckets_(num_buckets, std::numeric_limits<V>::max()) {}

  void assign(T num_buckets) {
    num_buckets_ = num_buckets;
    size_        = 0;
    buckets_.assign(num_buckets, std::numeric_limits<V>::max());
  }

  T    capacity() const { return num_buckets_; }

  T    size() const { return size_; }

  void clear() {
    buckets_.assign(num_buckets_, std::numeric_limits<V>::max());
    size_ = 0;
  }

  bool add(T key, V value) {
    if (key < 0 || key >= num_buckets_ || buckets_[key] != std::numeric_limits<V>::max()) return false;
    buckets_[key] = value;
    size_++;
    return true;
  }

  bool exists(T key) const {
    if (key < 0 || key >= num_buckets_) return false;
    return buckets_[key] != std::numeric_limits<V>::max();
  }

  V get(T key) const {
    if (key < 0 || key >= num_buckets_) return std::numeric_limits<V>::max();
    return buckets_[key];
  }

  T smallestEmptyBucket() const {
    for (T i = 0; i < num_buckets_; i++) {
      if (buckets_[i] == std::numeric_limits<V>::max()) return i;
    }
    return std::numeric_limits<T>::max();
  }

 protected:
  T              num_buckets_;
  T              size_;
  std::vector<V> buckets_;
};

template<class T>
void DispatchedIoLegalizerForward(database::PlaceDB const &placedb,
        int32_t                                            num_movable_ids,
        int32_t                                            num_fixed_ids,
        T                                                 *pos,
        T                                                 *pos_xyz,
        T                                                 *inst_locs_xyz,
        T                                                 *inst_sizes_max,
        int32_t                                           *movable_ids,
        int32_t                                           *fixed_ids,
        int32_t                                            area_type_id) {
  using IndexType                                                             = database::PlaceDB::IndexType;
  using Bucket                                                                = BucketCounter<IndexType, IndexType>;

  auto const                                                          &layout = placedb.db()->layout();
  auto const                                                          &design = placedb.db()->design();
  auto const                                                          &place_params = placedb.place_params();
  int32_t                                                              rsc_id = SelectResource(placedb, area_type_id);
  SiteRefVec                                                           concerned_sites = CollectBoxes(placedb, rsc_id);
  int32_t                                                              num_concerned_sites = concerned_sites.size();
  int32_t                                                              rsc_capacity;
  std::unordered_map<IndexType, std::unordered_map<IndexType, Bucket>> site_to_inst_count;

  for (int32_t i = 0; i < num_concerned_sites; i++) {
    const Site &site = concerned_sites[i];
    rsc_capacity     = placedb.getSiteCapacity(site, rsc_id);
    break;
  }

  openparfPrint(kInfo, "area type: %s(%d)\n", place_params.area_type_names_[area_type_id].c_str(), area_type_id);
  openparfPrint(kInfo, "resouce type: %d\n", rsc_id);
  openparfPrint(kInfo, "#movable insts: %d\n", num_movable_ids);
  openparfPrint(kInfo, "#fixed insts: %d\n", num_fixed_ids);
  openparfPrint(kInfo, "#sites with resource %d: %d\n", rsc_id, num_concerned_sites);
  openparfPrint(kInfo, "site capacity of resource %d: %d\n", rsc_id, rsc_capacity);

  if (num_movable_ids == 0) {
    openparfPrint(kInfo, "no movable instances\n");
    return;
  }

  // the site capacity excludes the fixed instances
  for (int32_t i = 0; i < num_fixed_ids; i++) {
    int32_t   inst_id = fixed_ids[i];
    IndexType xx      = static_cast<IndexType>(inst_locs_xyz[inst_id * 3]);
    IndexType yy      = static_cast<IndexType>(inst_locs_xyz[inst_id * 3 + 1]);
    IndexType zz      = static_cast<IndexType>(inst_locs_xyz[inst_id * 3 + 2]);
    Bucket   &bucket  = site_to_inst_count[xx][yy];
    if (bucket.capacity() == 0) {
      // a new site
      bucket.assign(rsc_capacity);
    }
    if (bucket.exists(zz)) {
      openparfPrint(kError, "fixed instance %d(%d, %d, %d) is placed on site (%d, %d) with %d fixed instances\n",
              inst_id, xx, yy, zz, xx, yy, bucket.get(zz));
      continue;
    }
    bucket.add(zz, inst_id);
    // openparfDebugMileStoneMsg("fixed instance %d %s(%d, %d, %d)", inst_id, placedb.instName(inst_id).c_str(), xx, yy,
    // zz);
  }

  // min-cost flow-based legalization
  using GraphType    = lemon::ListDigraph;
  using CapacityType = int32_t;
  using CostType     = int64_t;
  using SolverType   = lemon::NetworkSimplex<GraphType, CapacityType, CostType>;
  using ResultType   = typename SolverType::ProblemType;
  CostType const                                            kScale = 100;

  GraphType                                                 graph;
  GraphType::ArcMap<CapacityType>                           capacity_lower_bound(graph), capacity_upper_bound(graph);
  GraphType::ArcMap<CostType>                               cost(graph);
  SolverType                                                solver(graph);
  GraphType::Node                                           source, drain;
  std::vector<GraphType::Node>                              instance_nodes, site_nodes;
  std::vector<GraphType::Arc>                               instance_arcs, site_arcs;
  std::vector<std::tuple<int32_t, int32_t, GraphType::Arc>> inst_to_site_arcs;

  source = graph.addNode();
  drain  = graph.addNode();

  instance_nodes.clear();
  instance_arcs.clear();
  CapacityType total_supply = 0;
  for (int32_t i = 0; i < num_movable_ids; i++) {
    instance_nodes.emplace_back(graph.addNode());
    auto &node = instance_nodes.back();
    instance_arcs.emplace_back(graph.addArc(source, node));
    auto &arc                 = instance_arcs.back();
    capacity_lower_bound[arc] = 0;
    capacity_upper_bound[arc] = 1;
    cost[arc]                 = 0;
    total_supply += 1;
  }

  site_nodes.clear();
  site_arcs.clear();
  CapacityType total_capacity = 0;
  for (int32_t i = 0; i < num_concerned_sites; i++) {
    const Site &site = concerned_sites[i];
    site_nodes.emplace_back(graph.addNode());
    auto &node = site_nodes.back();
    site_arcs.emplace_back(graph.addArc(node, drain));
    auto        &arc          = site_arcs.back();
    CapacityType capacity     = rsc_capacity - site_to_inst_count[site.bbox().xl()][site.bbox().yl()].size();
    capacity_lower_bound[arc] = 0;
    capacity_upper_bound[arc] = capacity;
    cost[arc]                 = 0;
    total_capacity += capacity;
  }

  openparfPrint(kInfo, "total Supply: %d\n", total_supply);
  openparfPrint(kInfo, "total Capacity: %d\n", total_capacity);
  openparfAssert(total_supply <= total_capacity);

  inst_to_site_arcs.clear();
  for (int i = 0; i < num_movable_ids; i++) {
    for (int j = 0; j < num_concerned_sites; j++) {
      int32_t     inst_id       = movable_ids[i];
      auto        instance_node = instance_nodes[i];
      const Site &site          = concerned_sites[j];
      auto        site_node     = site_nodes[j];

      T           xx            = pos[inst_id << 1];
      T           yy            = pos[inst_id << 1 | 1];
      T           site_x        = (site.bbox().xl() + site.bbox().xh()) * 0.5;
      T           site_y        = (site.bbox().yl() + site.bbox().yh()) * 0.5;
      CostType    dist          = (std::fabs(site_x - xx) + std::fabs(site_y - yy)) * kScale;

      inst_to_site_arcs.emplace_back(i, j, graph.addArc(instance_node, site_node));
      auto &arc                 = std::get<2>(inst_to_site_arcs.back());
      capacity_lower_bound[arc] = 0;
      capacity_upper_bound[arc] = 1;
      cost[arc]                 = dist;
    }
  }

  solver.reset();
  auto rv = solver.stSupply(source, drain, total_supply)
                    .lowerMap(capacity_lower_bound)
                    .upperMap(capacity_upper_bound)
                    .costMap(cost)
                    .run();

  CapacityType total_flow = 0;
  for (auto const &arc : site_arcs) total_flow += solver.flow(arc);
  openparfAssert(rv == ResultType::OPTIMAL);
  openparfAssert(total_flow == total_supply);
  std::vector<T> cost_list;

  for (const auto &inst_site_arc : inst_to_site_arcs) {
    int32_t     inst_id = movable_ids[std::get<0>(inst_site_arc)];
    const Site &site    = concerned_sites[std::get<1>(inst_site_arc)];
    auto        arc     = std::get<2>(inst_site_arc);
    if (solver.flow(arc) > 0) {
      IndexType xx     = site.bbox().xl();
      IndexType yy     = site.bbox().yl();
      T         pos_x  = xx + inst_sizes_max[inst_id << 1] * 0.5;
      T         pos_y  = yy + inst_sizes_max[inst_id << 1 | 1] * 0.5;
      auto     &bucket = site_to_inst_count[xx][yy];
      if (bucket.capacity() == 0) {
        // a new site
        bucket.assign(rsc_capacity);
      }
      IndexType zz = bucket.smallestEmptyBucket();
      openparfAssert(zz != std::numeric_limits<IndexType>::max());
      cost_list.push_back(1.0 * cost[arc] / kScale);
      bucket.add(zz, inst_id);
      pos[inst_id << 1]        = pos_x;
      pos[inst_id << 1 | 1]    = pos_y;
      pos_xyz[inst_id * 3]     = pos_x;
      pos_xyz[inst_id * 3 + 1] = pos_y;
      pos_xyz[inst_id * 3 + 2] = zz;
      // openparfDebugMileStoneMsg("movable instance %s(%d, %d, %d) (%f, %f)", placedb.instName(inst_id).c_str(), xx,
      // yy,
      //         zz, pos_x, pos_y);
    }
  }
  openparfPrint(kInfo, "total cost: %f\n", std::accumulate(cost_list.begin(), cost_list.end(), 0.0));
  openparfPrint(kInfo, "average cost: %f\n",
          std::accumulate(cost_list.begin(), cost_list.end(), 0.0) / cost_list.size());
  openparfPrint(kInfo, "max cost: %f\n", *std::max_element(cost_list.begin(), cost_list.end()));
}

void IoLegalizerForward(database::PlaceDB const &placedb,
        at::Tensor                               pos,
        at::Tensor                               pos_xyz,
        at::Tensor                               inst_locs_xyz,
        at::Tensor                               inst_sizes_max,
        at::Tensor                               movable_inst_ids,
        at::Tensor                               fixed_inst_ids,
        int32_t                                  area_type_id) {
  CHECK_FLAT_CPU(pos);
  CHECK_EVEN(pos);
  CHECK_CONTIGUOUS(pos);
  CHECK_FLAT_CPU(pos_xyz);
  CHECK_CONTIGUOUS(pos_xyz);
  CHECK_FLAT_CPU(movable_inst_ids);
  CHECK_CONTIGUOUS(movable_inst_ids);
  AT_ASSERTM(movable_inst_ids.dtype() == torch::kInt32, "`movable_inst_ids` must be a Int32 tensor.");
  CHECK_FLAT_CPU(fixed_inst_ids);
  CHECK_CONTIGUOUS(fixed_inst_ids);
  AT_ASSERTM(fixed_inst_ids.dtype() == torch::kInt32, "`fixed_inst_ids` must be a Int32 tensor.");
  int32_t num_movable_insts = movable_inst_ids.size(0);
  int32_t num_fixed_insts   = fixed_inst_ids.size(0);
  OPENPARF_DISPATCH_FLOATING_TYPES(pos, "IoLegalizerForward", [&] {
    DispatchedIoLegalizerForward(placedb, num_movable_insts, num_fixed_insts, OPENPARF_TENSOR_DATA_PTR(pos, scalar_t),
            OPENPARF_TENSOR_DATA_PTR(pos_xyz, scalar_t), OPENPARF_TENSOR_DATA_PTR(inst_locs_xyz, scalar_t),
            OPENPARF_TENSOR_DATA_PTR(inst_sizes_max, scalar_t), OPENPARF_TENSOR_DATA_PTR(movable_inst_ids, int32_t),
            OPENPARF_TENSOR_DATA_PTR(fixed_inst_ids, int32_t), area_type_id);
  });
}

#define INSTANTIATE(T)                                                                                                 \
  template void DispatchedIoLegalizerForward(database::PlaceDB const &placedb, int32_t num_movable_ids,                \
          int32_t num_fixed_ids, T *pos, T *pos_xyz, T *inst_locs_xyz, T *inst_sizes_max, int32_t *movable_ids,        \
          int32_t *fixed_ids, int32_t area_type_id);


INSTANTIATE(float);
INSTANTIATE(double);

#undef INSTANTIATE

OPENPARF_END_NAMESPACE
