/**
 * @file net_density.cpp
 * @author Yifan Chen (chenyifan2019@pku.edu.cn)
 * @brief 
 * @version 0.1
 * @date 2023-08-18
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "util/torch.h"
#include "util/util.h"
#include "ops/net_density/src/net_density_kernel.hpp"

OPENPARF_BEGIN_NAMESPACE

void    net_density_forward(at::Tensor pin_pos,
           at::Tensor           netpin_start,
           at::Tensor           flat_netpin,
           at::Tensor           net_weights,
           double               bin_size_x,
           double               bin_size_y,
           double               xl,
           double               yl,
           double               xh,
           double               yh,
           int32_t              num_bins_x,
           int32_t              num_bins_y,
           int32_t              deterministic_flag,
           at::Tensor           horizontal_utilization_map,
           at::Tensor           vertical_utilization_map) {
     CHECK_FLAT_CPU(pin_pos);
     CHECK_EVEN(pin_pos);
     CHECK_CONTIGUOUS(pin_pos);

     CHECK_FLAT_CPU(netpin_start);
     CHECK_CONTIGUOUS(netpin_start);

     CHECK_FLAT_CPU(flat_netpin);
     CHECK_CONTIGUOUS(flat_netpin);

     CHECK_FLAT_CPU(net_weights);
     CHECK_CONTIGUOUS(net_weights);

     CHECK_FLAT_CPU(horizontal_utilization_map);
     CHECK_CONTIGUOUS(horizontal_utilization_map);

     CHECK_FLAT_CPU(vertical_utilization_map);
     CHECK_CONTIGUOUS(vertical_utilization_map);

     /**
      * |netpin_start| is similar to the IA array in CSR format, IA[i+1]-IA[i] is
      * the number of pins in each net, the length of IA is number of nets + 1
      */
     int32_t num_nets = netpin_start.numel() - 1;

     OPENPARF_DISPATCH_FLOATING_TYPES(pin_pos, "netDensityLauncher", [&] {
    netDensityLauncher<scalar_t>(OPENPARF_TENSOR_DATA_PTR(pin_pos, scalar_t), OPENPARF_TENSOR_DATA_PTR(netpin_start, int),
            OPENPARF_TENSOR_DATA_PTR(flat_netpin, int),
            net_weights.numel() > 0 ? OPENPARF_TENSOR_DATA_PTR(net_weights, scalar_t) : nullptr, bin_size_x, bin_size_y,
            xl, yl, xh, yh, num_bins_x, num_bins_y, num_nets, deterministic_flag, at::get_num_threads(),
            OPENPARF_TENSOR_DATA_PTR(horizontal_utilization_map, scalar_t),
            OPENPARF_TENSOR_DATA_PTR(vertical_utilization_map, scalar_t));
  });
}

OPENPARF_END_NAMESPACE

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
  m.def("forward", &OPENPARF_NAMESPACE::net_density_forward, "compute net density map");
}
