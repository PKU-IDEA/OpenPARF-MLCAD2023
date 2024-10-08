/**
 * @file net_density_cuda_kernel.cu
 * @author Yifan Chen (chenyifan2019@pku.edu.cn)
 * @brief 
 * @version 0.1
 * @date 2023-08-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <cuda_runtime.h>

#include "ops/net_density/src/risa_parameters.h"
#include "util/atomic_ops.cuh"
#include "util/limits.cuh"
#include "util/util.cuh"

OPENPARF_BEGIN_NAMESPACE

template<typename T>
inline __device__ DEFINE_NET_WIRING_DISTRIBUTION_MAP_WEIGHT;

template<typename T, typename U, typename AtomicOp>
__global__ void NetDensityCudaKernel(T *pin_pos,
        int32_t                  *netpin_start,
        int32_t                  *flat_netpin,
        T                        *net_weights,
        T                         bin_size_x,
        T                         bin_size_y,
        T                         xl,
        T                         yl,
        T                         xh,
        T                         yh,
        int32_t                   num_bins_x,
        int32_t                   num_bins_y,
        int32_t                   num_nets,
        AtomicOp                  atomic_op,
        U                        *horizontal_utilization_map,
        U                        *vertical_utilization_map) {
  int32_t i = threadIdx.x + blockDim.x * blockIdx.x;
  if (i < num_nets) {
    int32_t start = netpin_start[i];
    int32_t end   = netpin_start[i + 1];

    T       x_max = -cuda::numeric_limits<T>::max();
    T       x_min = cuda::numeric_limits<T>::max();
    T       y_max = -cuda::numeric_limits<T>::max();
    T       y_min = cuda::numeric_limits<T>::max();

    for (int32_t j = start; j < end; ++j) {
      int32_t pin_id = flat_netpin[j];
      T       xx     = pin_pos[pin_id << 1];
      T       yy     = pin_pos[(pin_id << 1) | 1];
      x_max          = OPENPARF_STD_NAMESPACE::max(xx, x_max);
      x_min          = OPENPARF_STD_NAMESPACE::min(xx, x_min);
      y_max          = OPENPARF_STD_NAMESPACE::max(yy, y_max);
      y_min          = OPENPARF_STD_NAMESPACE::min(yy, y_min);
    }

    // compute the bin box that this net will affect
    auto bin_index_xl = OPENPARF_STD_NAMESPACE::floor((x_min - xl) / bin_size_x);
    auto bin_index_xh = OPENPARF_STD_NAMESPACE::ceil((x_max - xl) / bin_size_x);
    auto bin_index_yl = OPENPARF_STD_NAMESPACE::floor((y_min - yl) / bin_size_y);
    auto bin_index_yh = OPENPARF_STD_NAMESPACE::ceil((y_max - yl) / bin_size_y);

    bin_index_xl      = OPENPARF_STD_NAMESPACE::max(bin_index_xl, (decltype(bin_index_xl)) 0);
    bin_index_xh      = OPENPARF_STD_NAMESPACE::min(bin_index_xh, (decltype(bin_index_xh)) num_bins_x);
    bin_index_yl      = OPENPARF_STD_NAMESPACE::max(bin_index_yl, (decltype(bin_index_yl)) 0);
    bin_index_yh      = OPENPARF_STD_NAMESPACE::min(bin_index_yh, (decltype(bin_index_yh)) num_bins_y);

    /**
      * Follow Wuxi's implementation, a tolerance is added to avoid
      * 0-size bounding box
      */
    T wt_h              = 1. / (bin_index_yh - bin_index_yl + cuda::numeric_limits<T>::epsilon());
    T wt_v              = 1. / (bin_index_xh - bin_index_xl + cuda::numeric_limits<T>::epsilon());

    if (net_weights) {
      wt_h *= net_weights[i];
      wt_v *= net_weights[i];
    }

    for (int32_t x = bin_index_xl; x < bin_index_xh; ++x) {
      for (int32_t y = bin_index_yl; y < bin_index_yh; ++y) {
        int32_t index = x * num_bins_y + y;
        atomic_op(horizontal_utilization_map + index, wt_h);
        atomic_op(vertical_utilization_map + index, wt_v);
      }
    }
  }
}

template<typename T>
void NetDensityCudaLauncher(T *pin_pos,
        int32_t         *netpin_start,
        int32_t         *flat_netpin,
        T               *net_weights,
        T                bin_size_x,
        T                bin_size_y,
        T                xl,
        T                yl,
        T                xh,
        T                yh,
        int32_t          num_bins_x,
        int32_t          num_bins_y,
        int32_t          num_nets,
        int32_t          deterministic_flag,
        T               *horizontal_utilization_map,
        T               *vertical_utilization_map) {
  if (deterministic_flag) {
    using AtomicIntType                   = unsigned long long int;
    AtomicIntType            scale_factor = 1e10;
    AtomicAdd<AtomicIntType> atomic_op(scale_factor);
    int32_t                  thread_count = 256;
    int32_t                  block_count  = ceilDiv(num_nets, thread_count);
    AtomicIntType           *buf_hmap     = nullptr;
    AtomicIntType           *buf_vmap     = nullptr;
    allocateCUDA(buf_hmap, num_bins_y * num_bins_x);
    allocateCUDA(buf_vmap, num_bins_y * num_bins_x);
    DEFER({
      destroyCUDA(buf_hmap);
      destroyCUDA(buf_vmap);
    });
    copyScaleArray<<<block_count, thread_count>>>(buf_hmap, horizontal_utilization_map, scale_factor,
            num_bins_y * num_bins_x);
    copyScaleArray<<<block_count, thread_count>>>(buf_vmap, vertical_utilization_map, scale_factor,
            num_bins_y * num_bins_x);

    NetDensityCudaKernel<T, AtomicIntType, decltype(atomic_op)>
            <<<(uint32_t) block_count, {(uint32_t) thread_count, 1u, 1u}>>>(pin_pos, netpin_start, flat_netpin,
                    net_weights, bin_size_x, bin_size_y, xl, yl, xh, yh, num_bins_x, num_bins_y, num_nets, atomic_op,
                    buf_hmap, buf_vmap);

    copyScaleArray<<<block_count, thread_count>>>(horizontal_utilization_map, buf_hmap,
            static_cast<T>(1.0 / scale_factor), num_bins_y * num_bins_x);
    copyScaleArray<<<block_count, thread_count>>>(vertical_utilization_map, buf_vmap,
            static_cast<T>(1.0 / scale_factor), num_bins_y * num_bins_x);
  } else {
    AtomicAdd<T> atomic_op;
    int32_t      thread_count = 256;
    int32_t      block_count  = ceilDiv(num_nets, thread_count);
    NetDensityCudaKernel<T, T, decltype(atomic_op)><<<(uint32_t) block_count, {(uint32_t) thread_count, 1u, 1u}>>>(pin_pos,
            netpin_start, flat_netpin, net_weights, bin_size_x, bin_size_y, xl, yl, xh, yh, num_bins_x, num_bins_y,
            num_nets, atomic_op, horizontal_utilization_map, vertical_utilization_map);
  }
}

// manually instantiate the template function
#define REGISTER_KERNEL_LAUNCHER(T)                                                                                    \
  template void NetDensityCudaLauncher<T>(T * pin_pos, int32_t * netpin_start, int32_t * flat_netpin, T * net_weights,       \
          T bin_size_x, T bin_size_y, T xl, T yl, T xh, T yh, int32_t num_bins_x, int32_t num_bins_y,                  \
          int32_t num_nets, int32_t deterministic_flag, T * horizontal_utilization_map, T * vertical_utilization_map);

REGISTER_KERNEL_LAUNCHER(float)
REGISTER_KERNEL_LAUNCHER(double)

#undef REGISTER_KERNEL_LAUNCHER

OPENPARF_END_NAMESPACE
