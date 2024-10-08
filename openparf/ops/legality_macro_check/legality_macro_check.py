#!/usr/bin/env python
# -*- coding:utf-8 -*-
###
# @file          : legality_macro_check.py
# @project       : legality_macro_check
# @author        : Jing Mai <jingmai@pku.edu.cn>
# @created date  : July 13 2023, 21:59:27, Thursday
# @brief         :
# -----
# Last Modified: July 17 2023, 21:53:14, Monday
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
from . import legality_macro_check_cpp


class LegalityMacroCheck(object):
    """
    @brief Check legality.
    """

    def __init__(
        self,
        placedb,
        data_cls,
        inst_check_ids,
        check_z_flag,
        check_region_flag,
        check_shape_flag,
    ):
        """
        @brief initialization
        """
        super(LegalityMacroCheck, self).__init__()
        self.placedb = placedb
        self.data_cls = data_cls
        self.inst_check_ids = inst_check_ids.cpu()
        self.check_z_flag = check_z_flag
        self.check_region_flag = check_region_flag
        self.check_shape_flag = check_shape_flag

    def forward(self, pos):
        local_pos = pos.cpu() if pos.is_cuda else pos
        rv = legality_macro_check_cpp.forward(
            self.placedb,
            self.inst_check_ids.to(torch.int32),
            self.check_z_flag,
            self.check_region_flag,
            self.check_shape_flag,
            local_pos,
        )
        return rv

    def __call__(self, pos):
        return self.forward(pos)
