/*
 * @file          : shape_alignment.h
 * @project       : src
 * @author        : Jing Mai <jingmai@pku.edu.cn>
 * @created date  : July 17 2023, 14:08:40, Monday
 * @brief         :
 * -----
 * Last Modified: July 17 2023, 14:31:17, Monday
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

#include <cstdint>

#include "util/namespace.h"

OPENPARF_BEGIN_NAMESPACE

namespace shape_alignment {

template<class T>
void        dispatchedForward(T *pos,
               T                *offset,
               int32_t          *index_data,
               int32_t          *index_start,
               int32_t           num_chains,
               int32_t           num_threads) {
#pragma omp parallel for num_threads(num_threads)
  for (int i = 0; i < num_chains; i++) {
    int32_t st            = index_start[i];
    int32_t en            = index_start[i + 1];
    T       inv_num_insts = 1.0 / (en - st);
    T       xx_sum        = 0;
    T       yy_sum        = 0;
    T       grav_xx, grav_yy;

    for (int j = st; j < en; j++) {
      int32_t inst_id = index_data[j];
      xx_sum += pos[inst_id << 1];
      yy_sum += pos[inst_id << 1 | 1];
    }
    grav_xx = xx_sum * inv_num_insts;
    grav_yy = yy_sum * inv_num_insts;
    for (int j = st; j < en; j++) {
      int32_t inst_id       = index_data[j];
      pos[inst_id << 1]     = grav_xx + offset[j << 1];
      pos[inst_id << 1 | 1] = grav_yy + offset[j << 1 | 1];
    }
  }
}
}   // namespace shape_alignment

OPENPARF_END_NAMESPACE
