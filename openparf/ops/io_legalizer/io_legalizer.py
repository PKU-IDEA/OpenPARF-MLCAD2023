#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# File              : io_legalizer.py
# Author            : Jing Mai <jingmai@pku.edu.cn>
# Date              : 08.24.2021
# Last Modified Date: 08.26.2021
# Last Modified By  : Jing Mai <jingmai@pku.edu.cn>
import logging
import torch
import pdb

from . import io_legalizer_cpp


class IoLegalizer(object):
    def __init__(
        self,
        placedb,
        data_cls,
        movable_inst_ids: torch.Tensor,
        fixed_inst_ids: torch.Tensor,
        area_type_id,
    ):
        assert movable_inst_ids.dtype == torch.int32
        assert not movable_inst_ids.is_cuda
        assert movable_inst_ids.is_contiguous
        assert fixed_inst_ids.dtype == torch.int32
        assert not fixed_inst_ids.is_cuda
        assert fixed_inst_ids.is_contiguous

        self.placedb = placedb
        self.data_cls = data_cls
        self.movable_inst_ids = movable_inst_ids
        self.fixed_inst_ids = fixed_inst_ids
        self.area_type_id = area_type_id

    def __call__(self, pos: torch.Tensor):
        local_pos = pos.cpu() if pos.is_cuda else pos
        pos_xyz = (
            torch.zeros((self.data_cls.movable_range[1], 3)).to(local_pos.dtype).cpu()
        )
        inst_locs_xyz = self.data_cls.inst_locs_xyz.to(local_pos.dtype).cpu()
        inst_sizes_max = self.data_cls.inst_sizes_max.to(local_pos.dtype).cpu()
        io_legalizer_cpp.forward(
            self.placedb,
            local_pos,
            pos_xyz,
            inst_locs_xyz,
            inst_sizes_max,
            self.movable_inst_ids,
            self.fixed_inst_ids,
            self.area_type_id,
        )
        if local_pos is not pos:
            with torch.no_grad():
                pos.data.copy_(local_pos)
        return pos_xyz.to(pos.device)
