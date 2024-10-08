#!/usr/bin/env python
# -*- coding:utf-8 -*-
###
# @file          : mixed_abacus_lg.py
# @project       : OpenPARF
# @author        : Jing Mai <jingmai@pku.edu.cn>
# @created date  : July 19 2023, 16:55:18, Wednesday
# @brief         :
# -----
# Last Modified: July 21 2023, 18:47:36, Friday
# Modified By: Jing Mai <jingmai@pku.edu.cn>
# -----
# @history :
# ====================================================================================
# Date         	By     	(version)	Comments
# -------------	-------	---------	--------------------------------------------------
# ====================================================================================
# Copyright (c) 2020 - 2023 All Right Reserved, PKU-IDEA Group
# -----
# This header is generated by VSCode extension psi-header.
###
import torch

from . import mixed_abacus_lg_cpp


class MixedAbacusLegalizer(object):
    def __init__(self, placedb, data_cls, lg_inst_ids: torch.Tensor, area_type_id):
        assert lg_inst_ids.dtype == torch.int32
        assert not lg_inst_ids.is_cuda
        assert lg_inst_ids.is_contiguous

        self.placedb = placedb
        self.data_cls = data_cls
        self.lg_inst_ids = lg_inst_ids.cpu()
        self.area_type_id = area_type_id

        assert data_cls.shape_inst_map is not None
        assert data_cls.region_inst_map is not None
        assert data_cls.region_boxes is not None

    def __call__(self, pos_xyz: torch.Tensor):
        local_pos_xyz = pos_xyz.cpu() if pos_xyz.is_cuda else pos_xyz
        mixed_abacus_lg_cpp.forward(
            self.placedb,
            local_pos_xyz,
            self.lg_inst_ids,
            self.area_type_id,
            self.data_cls.shape_inst_map.bs.cpu(),
            self.data_cls.shape_inst_map.b_starts.cpu(),
            self.data_cls.shape_inst_map.b2as.cpu(),
            self.data_cls.region_inst_map.b2as.cpu(),
            self.data_cls.region_boxes.cpu(),
        )
        if local_pos_xyz is not pos_xyz:
            with torch.no_grad():
                pos_xyz.data.copy_(local_pos_xyz)
