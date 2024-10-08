/*
 * @file          : legality_macro_check.hpp
 * @project       : src
 * @author        : Jing Mai <jingmai@pku.edu.cn>
 * @created date  : July 13 2023, 21:59:27, Thursday
 * @brief         :
 * -----
 * Last Modified: July 18 2023, 14:03:27, Tuesday
 * Modified By: Jing Mai <jingmai@pku.edu.cn>
 * -----
 * @history :
 * ====================================================================================
 * Date         	By     	(version)	Comments
 * -------------	-------	---------	--------------------------------------------------
 * ====================================================================================
 * Copyright (c) 2020 - 2023 All Right Reserved, PKU-IDEA Group
 * -----
 * This header is generated by VSCode extension psi-header.
 */
#ifndef OPENPARF_OPS_LEGALITY_MACRO_CHECK_SRC_LEGALITY_MACRO_CHECK_HPP_
#define OPENPARF_OPS_LEGALITY_MACRO_CHECK_SRC_LEGALITY_MACRO_CHECK_HPP_

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <vector>

// project headers
#include "container/vector_2d.hpp"
#include "database/placedb.h"
#include "geometry/box.hpp"
#include "util/torch.h"
#include "util/util.h"

OPENPARF_BEGIN_NAMESPACE

using container::Vector2D;

template<typename T, typename V>
bool isClose(T x, V y, double rtol = 1e-3, double atol = 1e-6) {
  auto delta = std::abs(x - y);
  return delta <= x * rtol || delta <= atol;
}

template<typename T>
bool legalityMacroCheck(database::PlaceDB const& db,
        int32_t*                                 inst_check_ids,
        int32_t                                  num_inst_check,
        bool                                     check_z_flag,        ///< whether check z location,
        bool                                     check_region_flag,   ///< whether check region constraints,
        bool                                     check_shape_flag,    ///< whether check shape constraints,
        T const*                                 pos   ///< xyzxyz, assume instances align to site lower left
) {
  using IndexType             = database::PlaceDB::IndexType;
  using Site                  = database::Site;

  bool        legal           = true;
  auto const& design          = db.db()->design();
  auto const& layout          = db.db()->layout();
  auto        top_module_inst = design.topModuleInst();
  openparfAssert(top_module_inst);
  auto const&         netlist      = top_module_inst->netlist();
  auto const&         site_map     = layout.siteMap();
  auto const&         resource_map = layout.resourceMap();

  // for easy query for valid site
  Vector2D<IndexType> valid_site_map(site_map.width(), site_map.height(), std::numeric_limits<IndexType>::max());
  for (auto const& site : site_map) {
    auto const& bbox = site.bbox();
    for (IndexType ix = bbox.xl(); ix < bbox.xh(); ++ix) {
      for (IndexType iy = bbox.yl(); iy < bbox.yh(); ++iy) {
        valid_site_map(ix, iy) = site_map.index1D(site.siteMapId().x(), site.siteMapId().y());
      }
    }
  }

  auto getValidSite = [&](IndexType ix, IndexType iy) {
    auto        id1d = valid_site_map(ix, iy);
    auto const& site = site_map.at(id1d);
    openparfAssert(site);
    return site;
  };

  auto getInstName = [&](IndexType inst_id) {
    // assume flat netlist for now
    auto const& inst = netlist.inst(db.oldInstId(inst_id));
    return inst.attr().name();
  };

  // Fill nodes into the site map and check overlapping and site compatibility
  Vector2D<std::vector<IndexType>> insts_in_site_map(site_map.width(), site_map.height());

  // check each instance and distribute instances to site map
  for (int i = 0; i < num_inst_check; ++i) {
    auto        inst_id   = inst_check_ids[i];
    auto const& inst      = netlist.inst(db.oldInstId(inst_id));
    auto const& model     = design.model(inst.attr().modelId());
    auto        inst_x    = pos[inst_id * 3];
    auto        inst_y    = pos[inst_id * 3 + 1];
    auto        inst_z    = pos[inst_id * 3 + 2];

    auto const& site      = *getValidSite(inst_x, inst_y);
    auto const& site_type = layout.siteType(site);

    // check align to site center
    auto        center_x  = (site.bbox().xl() + site.bbox().xh()) * 0.5;
    auto        center_y  = (site.bbox().yl() + site.bbox().yh()) * 0.5;
    if (!isClose(center_x, inst_x) || !isClose(center_y, inst_y)) {
      openparfPrint(kError, "inst %u (%s) @ (%g, %g, %g) failed to align to site (%u, %u) @ (%u, %u, %u, %u)\n",
              inst_id, inst.attr().name().c_str(), static_cast<double>(inst_x), static_cast<double>(inst_y),
              static_cast<double>(inst_z), site.siteMapId().x(), site.siteMapId().y(), site.bbox().xl(),
              site.bbox().yl(), site.bbox().xh(), site.bbox().yh());
      legal = false;
    }

    // check site type
    auto resource_ids    = resource_map.modelResourceIds(model.id());
    bool site_type_match = false;
    for (auto resource_id : resource_ids) {
      if (site_type.resourceCapacity(resource_id)) {
        site_type_match = true;
        break;
      }
    }
    if (!site_type_match) {
      openparfPrint(kError, "inst %u (%s) @ (%g, %g, %g) resource ", inst_id, inst.attr().name().c_str(),
              static_cast<double>(inst_x), static_cast<double>(inst_y), static_cast<double>(inst_z));
      for (auto resource_id : resource_ids) {
        auto const& resource = resource_map.resource(resource_id);
        openparfPrint(kNone, "%u (%s)", resource.id(), resource.name().c_str());
      }
      openparfPrint(kNone, " failed to match site (%u, %u) [");
      for (auto resource_id = 0U; resource_id < resource_map.numResources(); ++resource_id) {
        openparfPrint(kNone, " %u", site_type.resourceCapacity(resource_id));
      }
      openparfPrint(kNone, " ]\n");
      legal = false;
    }

    insts_in_site_map(site.siteMapId().x(), site.siteMapId().y()).push_back(inst_id);
  }

  // check each site
  for (auto const& site : site_map) {
    auto const& site_type = layout.siteType(site);
    auto&       inst_ids  = insts_in_site_map(site.siteMapId().x(), site.siteMapId().y());
    // sort according to z location
    std::sort(inst_ids.begin(), inst_ids.end(), [&](IndexType id1, IndexType id2) {
      auto z1 = pos[id1 * 3 + 2];
      auto z2 = pos[id2 * 3 + 2];
      return z1 < z2 || (z1 == z2 && id1 < id2);
    });

    std::vector<IndexType> resource_demands(resource_map.numResources(), 0);
    // compute resource demands taken by each instance in the site
    for (auto inst_id : inst_ids) {
      auto const& inst         = netlist.inst(db.oldInstId(inst_id));
      auto const& model        = design.model(inst.attr().modelId());
      auto        resource_ids = resource_map.modelResourceIds(model.id());
      for (auto resource_id : resource_ids) {
        if (site_type.resourceCapacity(resource_id)) {
          resource_demands[resource_id] += 1;
          break;
        }
      }
    }

    // check site capacity overflow
    bool fail_flag = false;
    for (auto resource_id = 0U; resource_id < resource_map.numResources(); ++resource_id) {
      if (resource_demands[resource_id] > site_type.resourceCapacity(resource_id)) {
        fail_flag = true;
        break;
      }
    }
    if (fail_flag) {
      openparfPrint(kError, "site (%u, %u) @ (%u, %u, %u, %u) [", site.siteMapId().x(), site.siteMapId().y(),
              site.bbox().xl(), site.bbox().yl(), site.bbox().xh(), site.bbox().yh());
      for (auto resource_id = 0U; resource_id < resource_map.numResources(); ++resource_id) {
        openparfPrint(kNone, " %u", site_type.resourceCapacity(resource_id));
      }
      openparfPrint(kNone, " ] resource overflow [");
      for (auto resource_id = 0U; resource_id < resource_map.numResources(); ++resource_id) {
        openparfPrint(kNone, " %u", resource_demands[resource_id]);
      }
      openparfPrint(kNone, " ] insts [");
      for (auto inst_id : inst_ids) {
        openparfPrint(kNone, " %u (%s)", inst_id, getInstName(inst_id).c_str());
      }
      openparfPrint(kNone, " ]\n");
      legal = false;
    }

    // check z location
    if (check_z_flag) {
      for (auto i = 1U; i < inst_ids.size(); ++i) {
        auto inst_id      = inst_ids[i];
        auto prev_inst_id = inst_ids[i - 1];
        auto inst_z       = pos[inst_id * 3 + 2];
        auto prev_inst_z  = pos[prev_inst_id * 3 + 2];
        if (prev_inst_z >= inst_z) {
          auto const& inst              = netlist.inst(db.oldInstId(inst_id));
          auto const& model             = design.model(inst.attr().modelId());
          auto        resource_ids      = resource_map.modelResourceIds(model.id());
          auto const& prev_inst         = netlist.inst(db.oldInstId(prev_inst_id));
          auto const& prev_model        = design.model(prev_inst.attr().modelId());
          auto        prev_resource_ids = resource_map.modelResourceIds(prev_model.id());
          std::sort(resource_ids.begin(), resource_ids.end());
          std::sort(prev_resource_ids.begin(), prev_resource_ids.end());

          // different resources, e.g., LUT/FF may have the same z locations
          if (resource_ids == prev_resource_ids) {
            openparfPrint(kError, "inst %u (%s) @ (%g, %g, %g) z-overlap with inst %u (%s) @ (%g, %g, %g)\n",
                    prev_inst_id, getInstName(prev_inst_id).c_str(), pos[prev_inst_id * 3], pos[prev_inst_id * 3 + 1],
                    pos[prev_inst_id * 3 + 2], inst_id, getInstName(inst_id).c_str(), pos[inst_id * 3],
                    pos[inst_id * 3 + 1], pos[inst_id * 3 + 2]);
            legal = false;
          }
        }
      }
    }
  }

  // check region
  if (check_region_flag) {
    auto const& design          = db.db()->design();
    auto const& layout          = db.db()->layout();
    auto        top_module_inst = design.topModuleInst();
    openparfAssert(top_module_inst);
    // assume flat netlist for now
    auto const&                 netlist           = top_module_inst->netlist();
    auto const&                 region_constraint = db.db()->regionConstraint();
    auto const&                 region_list       = region_constraint.region_list();
    std::unordered_set<int32_t> check_ids_set(inst_check_ids, inst_check_ids + num_inst_check);

    IndexType                   num_regions = region_list.size();
    for (IndexType region_id = 0; region_id < num_regions; ++region_id) {
      auto const& region = region_list[region_id];
      for (auto const& old_inst_id : region.instIds()) {
        IndexType new_inst_id = db.newInstId(old_inst_id);
        if (check_ids_set.find(new_inst_id) == check_ids_set.end()) {
          // macro check only
          continue;
        }
        auto        inst_x  = pos[new_inst_id * 3];
        auto        inst_y  = pos[new_inst_id * 3 + 1];
        auto        inst_z  = pos[new_inst_id * 3 + 2];
        auto const& site    = *getValidSite(inst_x, inst_y);
        IndexType   site_xl = site.bbox().xl();
        IndexType   site_yl = site.bbox().yl();

        if (!(region.bbox().xl() <= site_xl && site_xl < region.bbox().xh() && region.bbox().yl() <= site_yl &&
                    site_yl < region.bbox().yh())) {
          openparfPrint(kError, "inst %u (%s) @ (%g, %g, %g) failed to match region %u @ (%u, %u, %u, %u)\n",
                  new_inst_id, getInstName(new_inst_id).c_str(), static_cast<double>(inst_x),
                  static_cast<double>(inst_y), static_cast<double>(inst_z), region_id, region.bbox().xl(),
                  region.bbox().yl(), region.bbox().xh(), region.bbox().yh());
          legal = false;
        }
      }
    }
  }

  // check shape
  if (check_shape_flag) {
    auto const& design          = db.db()->design();
    auto const& layout          = db.db()->layout();
    auto        top_module_inst = design.topModuleInst();
    openparfAssert(top_module_inst);
    // assume flat netlist for now
    auto const& netlist          = top_module_inst->netlist();
    auto const& shape_constraint = db.db()->shapeConstraint();
    auto const& shapes           = shape_constraint.shapes();

    for (auto& shape : shapes) {
      std::vector<IndexType>                          new_inst_ids;
      std::vector<container::ObserverPtr<const Site>> inst_sites;
      new_inst_ids.reserve(shape.instIds().size());
      inst_sites.reserve(shape.instIds().size());
      std::transform(shape.instIds().begin(), shape.instIds().end(), std::back_inserter(new_inst_ids),
              [&](IndexType old_inst_id) { return db.newInstId(old_inst_id); });
      std::transform(new_inst_ids.begin(), new_inst_ids.end(), std::back_inserter(inst_sites),
              [&](IndexType new_inst_id) {
                auto        inst_x = pos[new_inst_id * 3];
                auto        inst_y = pos[new_inst_id * 3 + 1];
                auto const& site   = *getValidSite(inst_x, inst_y);

                return &site;
              });
      for (IndexType i = 0; i < shape.instIds().size() - 1; i++) {
        auto& cur_site      = *inst_sites[i];
        auto& next_site     = *inst_sites[i + 1];
        // check if the preceding site of `next_site` is `cur_site`
        auto& pre_next_site = *getValidSite(next_site.bbox().xl(), next_site.bbox().yl() - 1);
        if (pre_next_site.siteMapId() != cur_site.siteMapId()) {
          openparfPrint(kError,
                  "inst %u (%s) @ (%g, %g, %g) and inst %u (%s) @ (%g, %g, %g) failed to match shape %u\n",
                  new_inst_ids[i], getInstName(new_inst_ids[i]).c_str(), static_cast<double>(pos[new_inst_ids[i] * 3]),
                  static_cast<double>(pos[new_inst_ids[i] * 3 + 1]), static_cast<double>(pos[new_inst_ids[i] * 3 + 2]),
                  new_inst_ids[i + 1], getInstName(new_inst_ids[i + 1]).c_str(),
                  static_cast<double>(pos[new_inst_ids[i + 1] * 3]),
                  static_cast<double>(pos[new_inst_ids[i + 1] * 3 + 1]),
                  static_cast<double>(pos[new_inst_ids[i + 1] * 3 + 2]), shape.id());
          legal = false;
        }
      }
    }
  }

  return legal;
}

OPENPARF_END_NAMESPACE

#endif   // OPENPARF_OPS_LEGALITY_MACRO_CHECK_SRC_LEGALITY_MACRO_CHECK_HPP_
