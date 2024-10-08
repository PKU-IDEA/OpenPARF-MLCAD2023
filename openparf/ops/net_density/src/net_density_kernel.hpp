/**
 * @file net_density_kernel.hpp
 * @author Yifan Chen (chenyifan2019@pku.edu.cn)
 * @brief 
 * @version 0.1
 * @date 2023-08-18
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef OPENPARF_OPS_NET_DENSITY_SRC_NET_DENSITY_KERNEL_HPP_
#define OPENPARF_OPS_NET_DENSITY_SRC_NET_DENSITY_KERNEL_HPP_

#include <algorithm>
#include <cmath>
#include <limits>

#include "ops/net_density/src/risa_parameters.h"
#include "util/atomic_ops.h"
#include "util/util.h"

OPENPARF_BEGIN_NAMESPACE

template<typename T>
inline DEFINE_NET_WIRING_DISTRIBUTION_MAP_WEIGHT;

template<typename T, typename V, typename AtomicOp>
void netDensityKernel(const T*         pin_pos,
        const V*                 netpin_start,
        const V*                 flat_netpin,
        const T*                 net_weights,
        const T                  bin_size_x,
        const T                  bin_size_y,
        const T                  xl,
        const T                  yl,
        const T                  xh,
        const T                  yh,
        const V                  num_bins_x,
        const V                  num_bins_y,
        const V                  num_nets,
        AtomicOp                 atomic_op,
        const V                  num_threads,
        typename AtomicOp::type* horizontal_utilization_map,
        typename AtomicOp::type* vertical_utilization_map) {
  const T inv_bin_size_x = 1.0 / bin_size_x;
  const T inv_bin_size_y = 1.0 / bin_size_y;
  int32_t chunk_size     = OPENPARF_STD_NAMESPACE::max(int32_t(num_nets / num_threads / 16), 1);

#pragma omp parallel for num_threads(num_threads) schedule(static, chunk_size)
  for (int32_t i = 0; i < (int32_t) num_nets; i++) {
    T x_max = std::numeric_limits<T>::lowest();
    T x_min = std::numeric_limits<T>::max();
    T y_max = std::numeric_limits<T>::lowest();
    T y_min = std::numeric_limits<T>::max();

    for (auto j = netpin_start[i]; j < netpin_start[i + 1]; j++) {
      auto    pin_id = flat_netpin[j];
      // |pin_pos| is in the form of xyxyxy...
      const T xx     = pin_pos[pin_id << 1];
      const T yy     = pin_pos[(pin_id << 1) + 1];
      x_max          = OPENPARF_STD_NAMESPACE::max(x_max, xx);
      x_min          = OPENPARF_STD_NAMESPACE::min(x_min, xx);
      y_max          = OPENPARF_STD_NAMESPACE::max(y_max, yy);
      y_min          = OPENPARF_STD_NAMESPACE::min(y_min, yy);
    }

    // compute the bin box that this net will affect
    auto bin_index_xl = OPENPARF_STD_NAMESPACE::floor((x_min - xl) * inv_bin_size_x);
    auto bin_index_xh = OPENPARF_STD_NAMESPACE::ceil((x_max - xl) * inv_bin_size_x);
    auto bin_index_yl = OPENPARF_STD_NAMESPACE::floor((y_min - yl) * inv_bin_size_y);
    auto bin_index_yh = OPENPARF_STD_NAMESPACE::ceil((y_max - yl) * inv_bin_size_y);

    bin_index_xl      = OPENPARF_STD_NAMESPACE::max(bin_index_xl, (decltype(bin_index_xl)) 0);
    bin_index_xh      = OPENPARF_STD_NAMESPACE::min(bin_index_xh, (decltype(bin_index_xh)) num_bins_x);
    bin_index_yl      = OPENPARF_STD_NAMESPACE::max(bin_index_yl, (decltype(bin_index_yl)) 0);
    bin_index_yh      = OPENPARF_STD_NAMESPACE::min(bin_index_yh, (decltype(bin_index_yh)) num_bins_y);

    /**
      * Follow Wuxi's implementation, a tolerance is added to avoid
      * 0-size bounding box
      */
    T wt_h              = 1. / (bin_index_yh - bin_index_yl + std::numeric_limits<T>::epsilon());
    T wt_v              = 1. / (bin_index_xh - bin_index_xl + std::numeric_limits<T>::epsilon());

    if (net_weights) {
      wt_h *= net_weights[i];
      wt_v *= net_weights[i];
    }

    for (auto x = bin_index_xl; x < bin_index_xh; x++) {
      for (auto y = bin_index_yl; y < bin_index_yh; y++) {
        int32_t index = x * num_bins_y + y;
        atomic_op(&horizontal_utilization_map[index], wt_h);
        atomic_op(&vertical_utilization_map[index], wt_v);
      }
    }
  }
}

template<typename T, typename V>
void netDensityLauncher(const T* pin_pos,
        const V*           netpin_start,
        const V*           flat_netpin,
        const T*           net_weights,
        const T            bin_size_x,
        const T            bin_size_y,
        const T            xl,
        const T            yl,
        const T            xh,
        const T            yh,
        const V            num_bins_x,
        const V            num_bins_y,
        const V            num_nets,
        int32_t            deterministic_flag,
        int32_t            num_threads,
        T*                 horizontal_utilization_map,
        T*                 vertical_utilization_map) {
  if (deterministic_flag) {
    using AtomicIntType                   = int64_t;
    AtomicIntType            scale_factor = 1e10;
    AtomicAdd<AtomicIntType> atomic_op(scale_factor);

    AtomicIntType*           buf_hmap = new AtomicIntType[num_bins_x * num_bins_y];
    AtomicIntType*           buf_vmap = new AtomicIntType[num_bins_x * num_bins_y];
    DEFER({
      delete[] buf_hmap;
      delete[] buf_vmap;
    });

    copyScaleArray(buf_hmap, horizontal_utilization_map, scale_factor, num_bins_x * num_bins_y, num_threads);
    copyScaleArray(buf_vmap, vertical_utilization_map, scale_factor, num_bins_x * num_bins_y, num_threads);

    netDensityKernel<T, V, decltype(atomic_op)>(pin_pos, netpin_start, flat_netpin, net_weights, bin_size_x, bin_size_y, xl,
            yl, xh, yh, num_bins_x, num_bins_y, num_nets, atomic_op, num_threads, buf_hmap, buf_vmap);

    copyScaleArray(horizontal_utilization_map, buf_hmap, static_cast<T>(1.0 / scale_factor), num_bins_x * num_bins_y,
            num_threads);
    copyScaleArray(vertical_utilization_map, buf_vmap, static_cast<T>(1.0 / scale_factor), num_bins_x * num_bins_y,
            num_threads);

  } else {
    AtomicAdd<T> atomic_op;
    netDensityKernel<T, V, decltype(atomic_op)>(pin_pos, netpin_start, flat_netpin, net_weights, bin_size_x, bin_size_y, xl,
            yl, xh, yh, num_bins_x, num_bins_y, num_nets, atomic_op, num_threads, horizontal_utilization_map,
            vertical_utilization_map);
  }
}
OPENPARF_END_NAMESPACE

#endif   // OPENPARF_OPS_NET_DENSITY_SRC_NET_DENSITY_KERNEL_HPP_
