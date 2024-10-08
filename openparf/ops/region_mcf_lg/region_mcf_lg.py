#!/usr/bin/env python
# -*- coding:utf-8 -*-
###
# @file          : region_mcf_lg.py
# @project       : OpenPARF
# @author        : Jing Mai <jingmai@pku.edu.cn>
# @created date  : July 22 2023, 15:28:05, Saturday
# @brief         :
# -----
# Last Modified: July 23 2023, 14:13:28, Sunday
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
import logging

import torch

from . import region_mcf_lg_cpp
from ...py_config import rule

logger = logging.getLogger(__name__)


class regionMcfLegalizer(object):
    def __init__(self, params, placedb, data_cls):
        self.params = params
        self.placedb = placedb
        self.data_cls = data_cls
        if self.params.dtype == "float64":
            self.legalizer = region_mcf_lg_cpp.RegionMcfLegalizer_double(placedb)
            self.dtype = torch.float64
        elif self.params.dtype == "float32":
            self.legalizer = region_mcf_lg_cpp.RegionMcfLegalizer_float(placedb)
            self.dtype = torch.float32
        else:
            assert False, "Unsupported dtype %s" % self.params.dtype
        self.inst_ids_groups = []
        data_cls.ssr_area_types = []

        layout = placedb.db().layout()
        for site_type in layout.siteTypeMap():
            resource_id = rule.is_single_site_single_resource_type(
                site_type, placedb.db()
            )
            if resource_id is None:
                # This site type is not `SSSR`(single site, single resource).
                continue
            # As one model may correspond to different resources,
            # use the average area of different resources.
            inst_ids = placedb.collectInstIds(resource_id).tolist()
            site_boxes = placedb.collectSiteBoxes(resource_id).tolist()
            site_boxes = torch.tensor(site_boxes, dtype=data_cls.wl_precond.dtype)
            self.legalizer.add_sssir_instances(
                torch.tensor(inst_ids, dtype=torch.int32),
                data_cls.wl_precond.cpu(),
                site_boxes,
            )
            # Add fixed and movable instances for instances local masks.
            self.inst_ids_groups.append(inst_ids)

            area_types = placedb.resourceAreaTypes(
                layout.resourceMap().resource(resource_id)
            )
            for area_type in area_types:
                filler_bgn = (
                    data_cls.filler_range[0] + data_cls.num_fillers[:area_type].sum()
                )
                filler_end = (
                    data_cls.filler_range[0]
                    + data_cls.num_fillers[: area_type + 1].sum()
                )
                filler_range = (filler_bgn, filler_end)
                # Add filler instances for for instances lock masks.
                self.inst_ids_groups[-1].extend(range(filler_range[0], filler_range[1]))
                # Add area type lock masks.
                data_cls.ssr_area_types.append(area_type)
        logging.info("Single-site resource area types = %s" % data_cls.ssr_area_types)

    def __call__(self, pos):
        assert pos.dtype == self.dtype
        if pos.is_cuda:
            local_pos = pos.cpu()
        else:
            local_pos = pos
        is_legalized_insts = (
            (self.data_cls.shape_inst_map.b2as >= 0).cpu().to(torch.int32)
        )
        with torch.no_grad():
            res = self.legalizer.forward(local_pos, is_legalized_insts, self.placedb)
            pos.data.copy_(res)
            for i in range(len(self.inst_ids_groups)):
                self.data_cls.inst_lock_mask[self.inst_ids_groups[i]] = 1
            self.data_cls.area_type_lock_mask[self.data_cls.ssr_area_types] = 1
