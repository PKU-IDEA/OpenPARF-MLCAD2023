/*
 * @file          : shape_alignment.cpp
 * @project       : src
 * @author        : Jing Mai <jingmai@pku.edu.cn>
 * @created date  : July 17 2023, 14:08:40, Monday
 * @brief         :
 * -----
 * Last Modified: July 17 2023, 17:06:01, Monday
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

#include "shape_alignment_kernel.h"
#include "util/torch.h"
#include "util/util.h"

OPENPARF_BEGIN_NAMESPACE

namespace shape_alignment {

torch::Tensor buildInstShapeOffset(torch::Tensor shape2inst_data,
        torch::Tensor                            shape2inst_index_start,
        torch::Tensor                            inst_sizes) {
  CHECK_FLAT_CPU(shape2inst_data);
  CHECK_CONTIGUOUS(shape2inst_data);
  CHECK_TYPE(shape2inst_data, torch::kInt32);
  CHECK_FLAT_CPU(shape2inst_index_start);
  CHECK_CONTIGUOUS(shape2inst_index_start);
  CHECK_TYPE(shape2inst_index_start, torch::kInt32);
  CHECK_FLAT_CPU(inst_sizes);
  CHECK_CONTIGUOUS(inst_sizes);
  CHECK_EVEN(inst_sizes);

  int32_t       num_shapes            = shape2inst_index_start.numel() - 1;
  int32_t       num_insts             = shape2inst_data.numel();
  torch::Tensor offset_from_grav_core = torch::zeros({num_insts, 2}, torch::TensorOptions()
                                                                             .dtype(inst_sizes.dtype())
                                                                             .layout(torch::kStrided)
                                                                             .device(torch::kCPU)
                                                                             .requires_grad(false));

  auto          data                  = shape2inst_data.accessor<int32_t, 1>();
  auto          index_start           = shape2inst_index_start.accessor<int32_t, 1>();
  OPENPARF_DISPATCH_FLOATING_TYPES(inst_sizes, "build shape offset", [&] {
    auto sizes  = inst_sizes.accessor<scalar_t, 2>();
    auto offset = offset_from_grav_core.accessor<scalar_t, 2>();
    for (int32_t i = 0; i < num_shapes; i++) {
      int32_t  st         = index_start[i];
      int32_t  en         = index_start[i + 1];
      scalar_t height_sum = 0;
      for (int j = st; j < en; j++) {
        int32_t  inst_id = data[j];
        scalar_t height  = sizes[inst_id][1];
        height_sum += height;
      }
      scalar_t core_height = height_sum * 0.5;
      height_sum           = 0;
      for (int j = st; j < en; j++) {
        int32_t  inst_id = data[j];
        scalar_t height  = sizes[inst_id][1];
        offset[j][0]     = 0;
        offset[j][1]     = (height_sum + height * 0.5) - core_height;
        height_sum += height;
      }
    }
  });
  return offset_from_grav_core;
}

void forward(torch::Tensor pos,
        torch::Tensor      shape2inst_data,
        torch::Tensor      shape2inst_index_start,
        torch::Tensor      offset_from_grav_core,
        int32_t            num_threads) {
  CHECK_FLAT_CPU(pos);
  CHECK_CONTIGUOUS(pos);
  CHECK_FLAT_CPU(shape2inst_data);
  CHECK_CONTIGUOUS(shape2inst_data);
  CHECK_EQ(shape2inst_data.dtype(), torch::kInt32);
  CHECK_FLAT_CPU(shape2inst_index_start);
  CHECK_CONTIGUOUS(shape2inst_index_start);
  CHECK_EQ(shape2inst_index_start.dtype(), torch::kInt32);
  CHECK_FLAT_CPU(offset_from_grav_core);
  CHECK_CONTIGUOUS(offset_from_grav_core);
  CHECK_EQ(pos.dtype(), offset_from_grav_core.dtype());

  int32_t num_shapes = shape2inst_index_start.numel() - 1;

  OPENPARF_DISPATCH_FLOATING_TYPES(pos, "shape alignment forward", [&] {
    dispatchedForward<scalar_t>(OPENPARF_TENSOR_DATA_PTR(pos, scalar_t),
            OPENPARF_TENSOR_DATA_PTR(offset_from_grav_core, scalar_t),
            OPENPARF_TENSOR_DATA_PTR(shape2inst_data, int32_t),
            OPENPARF_TENSOR_DATA_PTR(shape2inst_index_start, int32_t), num_shapes, num_threads);
  });
}

#define REGISTER_KERNEL_LAUNCHER(T)                                                                                    \
  template void dispatchedForward<T>(T * pos, T * offset, int32_t * index_data, int32_t * index_start,                 \
          int32_t num_shapes, int32_t num_threads);

REGISTER_KERNEL_LAUNCHER(float)
REGISTER_KERNEL_LAUNCHER(double)

#undef REGISTER_KERNEL_LAUNCHER

}   // namespace shape_alignment

OPENPARF_END_NAMESPACE

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
  m.def("buildInstShapeOffset", &OPENPARF_NAMESPACE::shape_alignment::buildInstShapeOffset,
          "compute the offset from the shape gravity core.");
  m.def("forward", &OPENPARF_NAMESPACE::shape_alignment::forward, "shape alignment forward(CPU)");
}
