/*
 * @file          : shape_alignment_cuda.cpp
 * @project       : src
 * @author        : Jing Mai <jingmai@pku.edu.cn>
 * @created date  : July 17 2023, 14:08:40, Monday
 * @brief         :
 * -----
 * Last Modified: July 17 2023, 14:28:08, Monday
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

#include "util/torch.h"
#include "util/util.cuh"


OPENPARF_BEGIN_NAMESPACE

namespace shape_alignment {

template<class T>
void shapeAlignmentKernelLauncher(T *pos, T *offset, int32_t *index_data, int32_t *index_start, int32_t num_shapes);

void forward(at::Tensor pos,
        at::Tensor      shape2inst_data,
        at::Tensor      shape2inst_index_start,
        at::Tensor      offset_from_grav_core,
        int32_t         num_threads) {
  CHECK_FLAT_CUDA(pos);
  CHECK_CONTIGUOUS(pos);
  CHECK_FLAT_CUDA(shape2inst_data);
  CHECK_CONTIGUOUS(shape2inst_data);
  CHECK_TYPE(shape2inst_data, torch::kInt32);
  CHECK_FLAT_CUDA(shape2inst_index_start);
  CHECK_CONTIGUOUS(shape2inst_index_start);
  CHECK_TYPE(shape2inst_index_start, torch::kInt32);
  CHECK_FLAT_CUDA(offset_from_grav_core);
  CHECK_CONTIGUOUS(offset_from_grav_core);
  CHECK_EQ(pos.dtype(), offset_from_grav_core.dtype());

  int32_t num_shapes = shape2inst_index_start.numel() - 1;

  OPENPARF_DISPATCH_FLOATING_TYPES(pos, "shape alignment forward", [&] {
    int32_t thread_count = 256;
    int32_t block_count  = ceilDiv(num_shapes, thread_count);
    shapeAlignmentKernelLauncher<scalar_t>(OPENPARF_TENSOR_DATA_PTR(pos, scalar_t),
            OPENPARF_TENSOR_DATA_PTR(offset_from_grav_core, scalar_t),
            OPENPARF_TENSOR_DATA_PTR(shape2inst_data, int32_t),
            OPENPARF_TENSOR_DATA_PTR(shape2inst_index_start, int32_t), num_shapes);
  });
}


}   // namespace shape_alignment

OPENPARF_END_NAMESPACE

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
  m.def("forward", &OPENPARF_NAMESPACE::shape_alignment::forward, "shape alignment forward(CUDA)");
}
