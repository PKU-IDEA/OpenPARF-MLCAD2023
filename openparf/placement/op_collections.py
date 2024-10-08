#!/usr/bin/env python
# -*- coding:utf-8 -*-
###
# @file          : op_collections.py
# @project       : OpenPARF
# @author        : Yibo Lin <yibolin@pku.edu.cn>
# @created date  : March 30 2023, 21:43:59, Thursday
# @brief         :
# -----
# Last Modified: August 09 2023, 14:08:45, Wednesday
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
import math
import numpy as np
import torch
# from hummingbird.ml import convert, load

from ..ops.direct_lg import direct_lg
from ..ops.electric_potential import electric_potential
from ..ops.hpwl import hpwl
from ..ops.ism_dp import ism_dp
from ..ops.legality_check import legality_check
from ..ops.legality_macro_check import legality_macro_check
from ..ops.mcf_lg import mcf_lg
from ..ops.mixed_abacus_lg import mixed_abacus_lg
from ..ops.region_mcf_lg import region_mcf_lg
from ..ops.move_boundary import move_boundary
from ..ops.pin_pos import pin_pos
from ..ops.pin_utilization import pin_utilization
from ..ops.net_density import net_density
from ..ops.rudy import rudy
from ..ops.stable_div import stable_div
from ..ops.wawl import wawl
from ..ops.resource_area import resource_area
from ..ops.energy_well import energy_well
from ..ops.fence_region_checker import fence_region_checker
from ..ops.cr_ck_counter import cr_ck_counter
from ..ops import inst_cr_mover
from ..ops.utplace2_cnp import utplace2_cnp
from ..ops.io_legalizer import io_legalizer
from ..ops.chain_legalizer import chain_legalizer
from ..ops.chain_alignment import chain_alignment
from ..ops.shape_alignment import shape_alignment
from ..ops.region_alignment import region_alignment
from ..ops.delay_estimation import delay_estimation
from ..ops.static_timing_analysis import static_timing_analysis
# from ..ops.graph_builder import graph_builder

# from ..ops.congestion_prediction import congestion_prediction
from ..ops.masked_direct_lg import masked_direct_lg
from ..ops.ssr_abacus_lg import ssr_abacus_lg

logger = logging.getLogger(__name__)


def build_legality_check_op(params, placedb, data_cls):
    """legality check"""
    if params.confine_clock_region_flag:
        max_clk_per_clock_region = params.maximum_clock_per_clock_region
        max_clk_per_half_column = params.maximum_clock_per_half_column
    else:
        max_clk_per_clock_region = 0
        max_clk_per_half_column = 0

    return legality_check.LegalityCheck(
        placedb=placedb,
        data_cls=data_cls,
        check_z_flag=params.check_z_flag,
        max_clk_per_clock_region=max_clk_per_clock_region,
        max_clk_per_half_column=max_clk_per_half_column,
    )


def build_legality_macro_check_op(params, placedb, data_cls):
    ssr_ids = None
    for area_type in data_cls.ssr_area_types:
        t = data_cls.area_type_inst_groups[area_type]
        ssr_ids = t if ssr_ids is None else torch.cat([ssr_ids, t])
    ssr_ids = ssr_ids[ssr_ids < data_cls.movable_range[1]].long()
    return legality_macro_check.LegalityMacroCheck(
        placedb=placedb,
        data_cls=data_cls,
        inst_check_ids=ssr_ids,
        check_z_flag=params.check_z_flag,
        check_region_flag=params.check_region_flag,
        check_shape_flag=params.check_shape_flag,
    )


def build_stable_div_op():
    """Stable division that can handle divisor with zeros
    Mostly deal with x / 0 = 0 to avoid nan
    """
    return stable_div.StableDiv()


def build_stable_zero_div_op():
    """Stable division that can handle divisor with zeros
    Mostly deal with x / 0 = 0 to avoid nan
    """
    return stable_div.StableZeroDiv()


def build_electric_potential_op(params, placedb, data_cls):
    """Electric potential"""
    return electric_potential.ElectricPotential(
        inst_sizes=data_cls.inst_sizes,
        # inst_area_types=data_cls.inst_area_types,
        initial_density_maps=data_cls.initial_density_maps,
        bin_map_dims=torch.from_numpy(data_cls.bin_map_dims).to(
            data_cls.inst_sizes.device
        ),
        area_type_mask=data_cls.area_type_mask,
        xl=data_cls.diearea[0],
        yl=data_cls.diearea[1],
        xh=data_cls.diearea[2],
        yh=data_cls.diearea[3],
        movable_range=data_cls.movable_range,
        filler_range=data_cls.filler_range,
        fixed_range=data_cls.fixed_range,
        target_density=data_cls.target_density,
        smooth_flag=False,
        deterministic_flag=params.deterministic_flag,
        movable_macro_mask=None,
        fast_mode=False,
        xy_ratio=params.wirelength_weights[0] / params.wirelength_weights[1],
    )


def build_move_boundary_op(params, placedb, data_cls):
    """Move boundary"""
    return move_boundary.MoveBoundary(
        inst_sizes=data_cls.inst_sizes_max,
        xl=data_cls.diearea[0],
        yl=data_cls.diearea[1],
        xh=data_cls.diearea[2],
        yh=data_cls.diearea[3],
        movable_range=data_cls.movable_range,
        filler_range=data_cls.filler_range,
    )


def build_pin_pos_op(params, placedb, data_cls):
    """Compute pin position"""
    # use (0, 0) as pin offsets as we use pos as centers
    return pin_pos.PinPos(
        pin_offsets=torch.zeros_like(data_cls.pin_offsets),
        inst_pins=data_cls.inst_pin_map.bs,
        inst_pins_start=data_cls.inst_pin_map.b_starts,
        pin2inst_map=data_cls.inst_pin_map.b2as,
    )


def build_precond_op(params, placedb, data_cls):
    """Compute preconditioning"""

    wl_precond_means = data_cls.wl_precond.new_ones(data_cls.num_area_types)
    for area_type in range(data_cls.num_area_types):
        inst_ids = data_cls.area_type_inst_groups[area_type]
        if len(inst_ids):
            wl_precond_means[area_type] = data_cls.wl_precond[inst_ids].norm(p=1) / (
                len(inst_ids) - data_cls.num_fillers[area_type]
            )

    def precond_op(grad, alphas=None):
        with torch.no_grad():
            density_precond = data_cls.inst_areas.new_zeros(len(data_cls.inst_areas))
            if alphas is None:
                alphas = grad.new_ones(data_cls.num_area_types)
            else:
                alphas = alphas.clone().mul_(wl_precond_means)
            for area_type in range(data_cls.num_area_types):
                inst_ids = data_cls.area_type_inst_groups[area_type]
                lambda_areas = (
                    data_cls.inst_areas[inst_ids, area_type]
                    * data_cls.multiplier.lambdas[area_type]
                    * alphas[area_type]
                )
                density_precond[inst_ids] += lambda_areas
            # lambdas = torch.gather(input=data_cls.multiplier.lambdas_ext,
            #                       index=data_cls.inst_area_types_long,
            #                       dim=0)
            # density_precond = lambdas.mul_(data_cls.inst_areas)
            precond = (data_cls.wl_precond + density_precond).clamp_(min=1)
            grad.view([-1, 2]).div_(precond.view([-1, 1]))

    return precond_op


def build_direct_lg_op(params, placedb, data_cls):
    """Legalize LUTs and FFs"""
    return direct_lg.DirectLegalize(placedb=placedb, params=params)


def build_clock_region_aware_direct_lg_op(params, placedb, data_cls):
    return direct_lg.ClockAwareDirectLegalize(
        placedb=placedb,
        params=params,
        inst_to_clock_indexes=data_cls.inst_to_clock_indexes,
        max_clock_net_per_half_column=params.maximum_clock_per_half_column,
        honor_fence_region_constraints=False,
    )


def build_ism_dp_op(params, placedb, data_cls):
    """detailed placement with independent set matching"""
    return ism_dp.ISMDetailedPlace(
        placedb=placedb, params=params, honor_clock_constraints=False
    )


def build_rudy_op(params, data_cls):
    return rudy.Rudy(
        netpin_start=data_cls.net_pin_map.b_starts,
        flat_netpin=data_cls.net_pin_map.bs,
        net_weights=data_cls.net_weights,
        xl=data_cls.diearea[0],
        yl=data_cls.diearea[1],
        xh=data_cls.diearea[2],
        yh=data_cls.diearea[3],
        num_bins_x=math.ceil(
            (data_cls.diearea[2] - data_cls.diearea[0]) / params.routing_bin_size_x
        ),
        num_bins_y=math.ceil(
            (data_cls.diearea[3] - data_cls.diearea[1]) / params.routing_bin_size_y
        ),
        unit_horizontal_capacity=params.unit_horizontal_routing_capacity,
        unit_vertical_capacity=params.unit_vertical_routing_capacity,
        deterministic_flag=params.deterministic_flag,
        initial_horizontal_utilization_map=None,
        initial_vertical_utilization_map=None,
    )

def build_net_density_op(params, data_cls):
    return net_density.NetDensity(
        netpin_start=data_cls.net_pin_map.b_starts,
        flat_netpin=data_cls.net_pin_map.bs,
        net_weights=data_cls.net_weights,
        xl=data_cls.diearea[0],
        yl=data_cls.diearea[1],
        xh=data_cls.diearea[2],
        yh=data_cls.diearea[3],
        num_bins_x=math.ceil(
            (data_cls.diearea[2] - data_cls.diearea[0]) / params.routing_bin_size_x
        ),
        num_bins_y=math.ceil(
            (data_cls.diearea[3] - data_cls.diearea[1]) / params.routing_bin_size_y
        ),
        deterministic_flag=params.deterministic_flag,
        initial_horizontal_net_density_map=None,
        initial_vertical_net_density_map=None,
    )

def build_pin_utilization_op(params, data_cls):
    instpin_start = data_cls.inst_pin_map.b_starts
    # Derived from elfplace implementation in elfplace. Since control set pins(e.g., CK/CR/CE) in
    # FFs can be largely shared, it is almost always an overestimation of using FF's pin count
    # directly. To compensate this effect, we give FFs a pre-defined pin weight. For non-FF
    # instances, we simply use pin counts as the weights.
    inst_pin_weights = (instpin_start[1:] - instpin_start[:-1]).type(
        data_cls.inst_sizes.dtype
    )
    inst_pin_weights.masked_fill_(data_cls.is_inst_ffs.bool(), params.ff_pin_weight)

    # ignore filler instance
    return pin_utilization.PinUtilization(
        inst_pin_weights=inst_pin_weights,
        xl=data_cls.diearea[0],
        yl=data_cls.diearea[1],
        xh=data_cls.diearea[2],
        yh=data_cls.diearea[3],
        inst_range=(data_cls.movable_range[0], data_cls.fixed_range[1]),
        num_bins_x=math.ceil(
            (data_cls.diearea[2] - data_cls.diearea[0]) / params.pin_bin_size_x
        ),
        num_bins_y=math.ceil(
            (data_cls.diearea[3] - data_cls.diearea[1]) / params.pin_bin_size_y
        ),
        unit_pin_capacity=params.unit_pin_capacity,
        pin_stretch_ratio=params.pin_stretch_ratio,
        deterministic_flag=params.deterministic_flag,
    )


def build_resource_area_op(params, data_cls):
    xl = data_cls.diearea[0]
    yl = data_cls.diearea[1]
    xh = data_cls.diearea[2]
    yh = data_cls.diearea[3]
    num_insts = data_cls.is_inst_luts.size()[0]
    stddev = math.sqrt(2.5e-4 * num_insts) / (2.0 * params.gp_inst_dem_stddev_trunc)
    num_bins_x = math.ceil((xh - xl) / stddev)
    num_bins_y = math.ceil((yh - yl) / stddev)
    ff_ctrlsets, num_cksr, num_ce = resource_area.compute_control_sets(
        inst_pins=data_cls.inst_pin_map.bs.cpu(),
        inst_pins_start=data_cls.inst_pin_map.b_starts.cpu(),
        pin2net_map=data_cls.net_pin_map.b2as.cpu(),
        pin_signal_types=data_cls.pin_signal_types.cpu(),
        is_inst_ffs=data_cls.is_inst_ffs.cpu(),
    )
    if params.gp_adjust_packing_rule:
        gp_adjust_packing_rule = params.gp_adjust_packing_rule
    else:
        if params.architecture_name == "ultrascale":
            gp_adjust_packing_rule = "ultrascale"
        elif params.architecture_name == "xarch":
            gp_adjust_packing_rule = "xarch"
        else:
            assert False, "Unknown architecture name: %s" % params.architecture_name
    return resource_area.ResourceArea(
        is_inst_luts=data_cls.is_inst_luts.cpu(),
        is_inst_ffs=data_cls.is_inst_ffs.cpu(),
        ff_ctrlsets=ff_ctrlsets,
        num_cksr=num_cksr,
        num_ce=num_ce,
        num_bins_x=num_bins_x,
        num_bins_y=num_bins_y,
        stddev_x=stddev,
        stddev_y=stddev,
        stddev_trunc=params.gp_inst_dem_stddev_trunc,
        slice_capacity=params.CLB_capacity,
        gp_adjust_packing_rule=gp_adjust_packing_rule,
    )


def build_clock_network_planner_op(placedb, params):
    from ..ops.clock_network_planner import clock_network_planner

    return clock_network_planner.ClockNetworkPlanner(
        placedb, params.dtype, params.maximum_clock_per_clock_region
    )


def build_fence_region_op(data_cls, placedb):
    x_cr_num, y_cr_num = placedb.clockRegionMapSize()
    return energy_well.EnergyWell(
        well_boxes=data_cls.fence_region_boxes,
        energy_function_exponents=None,
        inst_areas=None,
        inst_sizes=None,
        num_crs=x_cr_num * y_cr_num,
        placedb=placedb,
        inst_cr_avail_map=None,
    )


def build_cr_ck_op(data_cls, placedb):
    x_cr_num, y_cr_num = placedb.clockRegionMapSize()
    return cr_ck_counter.CrCkCounter(
        inst_cks=data_cls.inst_to_clock_indexes,
        num_crs=x_cr_num * y_cr_num,
        num_cks=data_cls.num_clocks,
        placedb=placedb,
    )


def build_fence_region_checker_op(data_cls):
    return fence_region_checker.FenceRegionChecker(
        fence_region_boxes=data_cls.fence_region_boxes,
        inst_sizes=None,
        inst_avail_crs=None,
    )


def build_inst_cr_mover(data_cls):
    return inst_cr_mover.InstCrMover(
        cr_boxes=data_cls.fence_region_boxes,
    )


def build_utplace2_cnp_op(placedb):
    return utplace2_cnp.UTPlace2CNP(placedb=placedb)


def build_io_legalization_op(params, placedb, data_cls):
    if not params.io_at_names:
        return None
    io_at_names = params.io_at_names
    io_at_ids = [placedb.getAreaTypeIndexFromName(x) for x in io_at_names]
    io_legalizers = []
    for idx, io_at_id in enumerate(io_at_ids):
        inst_ids = data_cls.area_type_inst_groups[io_at_id]
        # select movable IO instances and drop fixed instances and fillers
        movable_inst_ids = inst_ids[
            torch.logical_and(
                data_cls.movable_range[0] <= inst_ids,
                inst_ids < data_cls.movable_range[1],
            )
        ]
        fixed_inst_ids = inst_ids[
            torch.logical_and(
                data_cls.fixed_range[0] <= inst_ids, inst_ids < data_cls.fixed_range[1]
            )
        ]
        logger.info(
            "IO Legalization: Area Type: %s(%d) movable: %d fixed: %d"
            % (
                io_at_names[idx],
                io_at_id,
                movable_inst_ids.numel(),
                fixed_inst_ids.numel(),
            )
        )
        movable_inst_ids = movable_inst_ids.cpu().to(torch.int32).contiguous()
        fixed_inst_ids = fixed_inst_ids.cpu().to(torch.int32).contiguous()
        legalizer = io_legalizer.IoLegalizer(
            placedb, data_cls, movable_inst_ids, fixed_inst_ids, io_at_id
        )
        io_legalizers.append(legalizer)

    def io_legalization_op(pos: torch.Tensor):
        with torch.no_grad():
            local_pos = pos.cpu() if pos.is_cuda else pos
            pos_xyz = None
            for legalizer_op in io_legalizers:
                t = legalizer_op(local_pos)
                pos_xyz = t if pos_xyz is None else t + pos_xyz
            pos.data.copy_(local_pos)
            return pos_xyz

    return io_legalization_op


def build_chain_legalization_op(params, placedb, data_cls):
    if not params.carry_chain_at_name:
        return None
    chain_at_name = params.carry_chain_at_name
    chain_at_id = placedb.getAreaTypeIndexFromName(chain_at_name)
    inst_ids = data_cls.area_type_inst_groups[chain_at_id]
    movable_inst_ids = inst_ids[
        torch.logical_and(
            data_cls.movable_range[0] <= inst_ids, inst_ids < data_cls.movable_range[1]
        )
    ]
    movable_inst_ids = movable_inst_ids.cpu().to(torch.int32).contiguous()
    max_iter = 50
    search_manh_dist_increment = math.ceil(
        (
            data_cls.diearea[2]
            - data_cls.diearea[0]
            + data_cls.diearea[3]
            - data_cls.diearea[1]
        )
        / max_iter
    )
    legalizer = chain_legalizer.ChainLegalizer(
        placedb,
        data_cls,
        movable_inst_ids,
        chain_at_id,
        search_manh_dist_increment,
        max_iter,
    )

    def chain_legalization_op(pos_xyz: torch.Tensor):
        with torch.no_grad():
            local_pos_xyz = pos_xyz.cpu() if pos_xyz.is_cuda else pos_xyz
            legalizer(local_pos_xyz)
            if local_pos_xyz is not pos_xyz:
                pos_xyz.data.copy_(local_pos_xyz)

    return chain_legalization_op


def build_chain_alignment_op(params, placedb, data_cls):
    if not params.align_carry_chain_flag:
        return None
    return chain_alignment.ChainAlignment(params=params, data_cls=data_cls)


def build_align_shape_op(params, placedb, data_cls):
    if not params.align_shape_flag:
        return None
    else:
        return shape_alignment.ShapeAlignment(params, placedb, data_cls)


def build_align_region_op(params, placedb, data_cls):
    if not params.align_region_flag:
        return None
    else:
        return region_alignment.RegionAlignment(params, placedb, data_cls)


# def build_congestion_prediction_op(params, data_cls):
#     return congestion_prediction.Congestion_prediction(
#         netpin_start=data_cls.net_pin_map.b_starts,
#         flat_netpin=data_cls.net_pin_map.bs,
#         net_weights=data_cls.net_weights,
#         xl=data_cls.diearea[0],
#         yl=data_cls.diearea[1],
#         xh=data_cls.diearea[2],
#         yh=data_cls.diearea[3],
#         num_bins_x=math.ceil(
#             (data_cls.diearea[2] - data_cls.diearea[0]) / params.routing_bin_size_x
#         ),
#         num_bins_y=math.ceil(
#             (data_cls.diearea[3] - data_cls.diearea[1]) / params.routing_bin_size_y
#         ),
#         unit_horizontal_capacity=params.unit_horizontal_routing_capacity,
#         unit_vertical_capacity=params.unit_vertical_routing_capacity,
#         pinDirects=data_cls.pin_signal_directs.double(),
#         initial_horizontal_utilization_map=None,
#         initial_vertical_utilization_map=None,
#         initial_pin_density_map=None,
#     )


def build_estimate_delay_op(params, data_cls, placedb):
    delay_model_path = params.delay_model_path
    delay_model = load(delay_model_path)
    return delay_estimation.DelayEstimation(
        placedb=placedb, data_cls=data_cls, delay_model=delay_model
    )


def build_static_timing_op(params, data_cls, placedb):
    timing_period = params.timing_period
    return static_timing_analysis.StaticTimingAnalysis(
        placedb=placedb, data_cls=data_cls, timing_period=timing_period
    )

def build_graph_builder_op(params, data_cls):
    return graph_builder.GraphBuilder(
        # todo: check num_inst is correct or not
        num_inst=data_cls.is_inst_luts.size()[0],
        inst_pins=data_cls.inst_pin_map.bs,
        inst_pins_start=data_cls.inst_pin_map.b_starts,
        pin2inst_map=data_cls.inst_pin_map.b2as,
        flat_netpin=data_cls.net_pin_map.bs,
        netpin_start=data_cls.net_pin_map.b_starts,
        pin2net_map=data_cls.net_pin_map.b2as,
    )

def build_ssr_abacus_legalize_op(params, data_cls, placedb):
    abacus_lg_resource_name = params.abacus_lg_resource_name
    if not abacus_lg_resource_name:
        return None
    resource_id = (
        placedb.db().layout().resourceMap().resourceId(abacus_lg_resource_name)
    )
    at_ids = placedb.resourceAreaTypes(
        placedb.db().layout().resourceMap().resource(resource_id)
    ).tolist()
    at_id = at_ids[0]
    inst_ids = torch.tensor(
        placedb.collectInstIds(resource_id).tolist(), dtype=torch.int32
    )
    movable_inst_ids = inst_ids[
        torch.logical_and(
            data_cls.movable_range[0] <= inst_ids, inst_ids < data_cls.movable_range[1]
        )
    ]
    movable_inst_ids = movable_inst_ids.cpu().to(torch.int32).contiguous()
    logger.info(
        "SSR Abacus-base Legalzation: #insts: %d    Area Type: (%d)"
        % (inst_ids.numel(), at_id)
    )
    return ssr_abacus_lg.SsrAbacusLegalizer(placedb, data_cls, movable_inst_ids, at_id)


def build_mixed_abacus_lg_op(params, data_cls, placedb):
    mixed_lg_resources = params.mixed_lg_resources
    if not mixed_lg_resources:
        return None
    legalizers = []
    for rsc_name in mixed_lg_resources:
        rsc_id = placedb.db().layout().resourceMap().resourceId(rsc_name)
        at_id = placedb.resourceAreaTypes(
            placedb.db().layout().resourceMap().resource(rsc_id)
        ).tolist()[0]
        at_name = placedb.place_params().area_type_names[at_id]
        inst_ids = torch.tensor(
            placedb.collectInstIds(rsc_id).tolist(), dtype=torch.int32
        )
        movable_inst_ids = inst_ids[
            torch.logical_and(
                data_cls.movable_range[0] <= inst_ids,
                inst_ids < data_cls.movable_range[1],
            )
        ]
        legalizer = mixed_abacus_lg.MixedAbacusLegalizer(
            placedb, data_cls, movable_inst_ids, at_id
        )
        legalizers.append(legalizer)
        logger.info(
            "Mixed-size Abacus-base Legalization: resoure: %s (%d), area type: %s (%d)"
            % (rsc_name, rsc_id, at_name, at_id)
        )

    def mixed_abacus_lg_op(pos):
        with torch.no_grad():
            for legalizer in legalizers:
                legalizer(pos)

    return mixed_abacus_lg_op


def build_region_mcf_lg(params, data_cls, placedb):
    return region_mcf_lg.regionMcfLegalizer(params, placedb, data_cls)


class OpCollections(object):
    """Collection of all operators for placement"""

    def __init__(self, params, placedb, data_cls):
        self.stable_div_op = build_stable_div_op()
        self.stable_zero_div_op = build_stable_zero_div_op()
        self.move_boundary_op = build_move_boundary_op(params, placedb, data_cls)
        self.random_pos_op = self.build_random_pos_op(params, placedb, data_cls)
        self.pin_pos_op = build_pin_pos_op(params, placedb, data_cls)
        self.hpwl_op = self.build_hpwl_op(params, placedb, data_cls)
        self.wirelength_op = self.build_wawl_op(params, placedb, data_cls)
        self.density_op = build_electric_potential_op(params, placedb, data_cls)
        self.overflow_op = self.build_electric_overflow_op(params, placedb, data_cls)
        self.normalized_overflow_op = self.build_normalized_overflow_op(
            params, placedb, data_cls
        )
        self.precond_op = build_precond_op(params, placedb, data_cls)
        # single-site resource
        # i.e., a resource occupies exactly one site
        self.ssr_legalize_op = mcf_lg.MinCostFlowLegalizer(params, placedb, data_cls)
        self.ssr_abacus_legalize_op = build_ssr_abacus_legalize_op(
            params, data_cls, placedb
        )

        if params.macro_place_flag:
            self.ssr_mixed_size_legalize_op = build_mixed_abacus_lg_op(
                params, data_cls, placedb
            )
        else:
            self.ssr_mixed_size_legalize_op = None

        if params.macro_place_flag:
            self.region_mcf_lg_op = build_region_mcf_lg(params, data_cls, placedb)
        else:
            self.region_mcf_lg_op = None

        # legalize LUTs and FFs
        self.direct_lg_op = (
            build_direct_lg_op(params, placedb, data_cls)
            if not params.confine_clock_region_flag
            else build_clock_region_aware_direct_lg_op(params, placedb, data_cls)
        )
        # detailed placement
        self.ism_dp_op = build_ism_dp_op(params, placedb, data_cls)
        # legality check
        self.legality_check_op = build_legality_check_op(params, placedb, data_cls)
        # legality check for macro placement
        self.legality_macro_check_op = build_legality_macro_check_op(
            params, placedb, data_cls
        )
        # routing utilization map
        self.net_density_op = build_net_density_op(params, data_cls)
        self.rudy_op = build_rudy_op(params, data_cls)
        # routability congestion prediction
        # if params.congestion_prediction_flag:
        #     self.congestion_prediction_op = build_congestion_prediction_op(
        #         params, data_cls
        #     )
        # else:
        self.congestion_prediction_op = None

        # self.graph_builder_op = build_graph_builder_op(params, data_cls)

        # pin utilization map
        self.pin_utilization_op = build_pin_utilization_op(params, data_cls)
        # resource area map
        self.resource_area_op = build_resource_area_op(params, data_cls)
        # energy well
        self.fence_region_op = build_fence_region_op(data_cls, placedb)
        # IO legalization
        self.io_legalization_op = build_io_legalization_op(params, placedb, data_cls)
        # fence region checker
        self.fence_region_checker_op = build_fence_region_checker_op(data_cls)
        if params.confine_clock_region_flag:
            self.clock_network_planner_op = build_clock_network_planner_op(
                placedb, params
            )
            self.cr_ck_counter_op = build_cr_ck_op(data_cls, placedb)
            self.inst_cr_mover_op = build_inst_cr_mover(data_cls)
            self.utplace2_cnp_op = build_utplace2_cnp_op(placedb)
        else:
            self.clock_network_planner_op = None
            self.cr_ck_counter_op = None
            self.inst_cr_mover_op = None
            self.utplace2_cnp_op = None
        self.chain_legalization_op = build_chain_legalization_op(
            params, placedb, data_cls
        )
        if params.carry_chain_module_name:
            self.masked_direct_lg_op = masked_direct_lg.DirectLegalize(
                placedb, data_cls, params
            )
        else:
            self.masked_direct_lg_op = None

        if params.align_carry_chain_flag:
            self.chain_alignment_op = build_chain_alignment_op(
                params, placedb, data_cls
            )
        else:
            self.chain_alignment_op = None

        if params.align_shape_flag:
            self.align_shape_op = build_align_shape_op(params, placedb, data_cls)
        else:
            self.align_shape_op = None

        if params.align_region_flag:
            self.align_region_op = build_align_region_op(params, placedb, data_cls)
        else:
            self.align_region_op = None

        if params.timing_optimization_flag or params.report_timing_flag:
            self.estimate_delay_op = build_estimate_delay_op(params, data_cls, placedb)
            self.static_timing_op = build_static_timing_op(params, data_cls, placedb)
        else:
            self.estimate_delay_op = None
            self.static_timing_op = None

    def build_random_pos_op(self, params, placedb, data_cls):
        def random_pos(pos):
            with torch.no_grad():
                # default center set to the center of placement region
                # fixed instances
                # use the average pin locations of fixed instances as the
                # initial center
                fixed_range = data_cls.fixed_range
                if fixed_range[0] < fixed_range[1]:
                    fixed_inst_num_pins = (
                        data_cls.inst_pin_map.b_starts[
                            fixed_range[0] + 1 : fixed_range[1] + 1
                        ]
                        - data_cls.inst_pin_map.b_starts[
                            fixed_range[0] : fixed_range[1]
                        ]
                    )
                    fixed_inst_num_pins = fixed_inst_num_pins.view([-1, 1]).type(
                        pos.dtype
                    )
                    init_center = (
                        pos[fixed_range[0] : fixed_range[1]] * fixed_inst_num_pins
                    ).sum(dim=0) / fixed_inst_num_pins.sum()
                else:
                    init_center = torch.tensor(
                        [
                            (data_cls.diearea[0] * 0.5 + data_cls.diearea[2] * 0.5),
                            (data_cls.diearea[1] * 0.5 + data_cls.diearea[3] * 0.5),
                        ]
                    )
                # movable instances
                movable_range = data_cls.movable_range
                min_wh = min(
                    (data_cls.diearea[2] - data_cls.diearea[0]),
                    (data_cls.diearea[3] - data_cls.diearea[1]),
                )
                init_pos = np.random.normal(
                    loc=init_center.cpu(),
                    scale=[min_wh * 0.001, min_wh * 0.001],
                    size=[movable_range[1] - movable_range[0], 2],
                )
                pos[movable_range[0] : movable_range[1]] = torch.from_numpy(
                    init_pos
                ).to(pos.device)

                # movble instances in regions
                # set the the center of each region as the initial center
                if params.random_region_center_init_flag and len(data_cls.region_boxes) > 0:
                    for region_id in range(len(data_cls.region_boxes)):
                        region_box = data_cls.region_boxes[region_id]
                        region_inst_bgn = data_cls.region_inst_map.b_starts[region_id]
                        region_inst_end = data_cls.region_inst_map.b_starts[region_id + 1]
                        region_inst_ids = data_cls.region_inst_map.bs[region_inst_bgn:region_inst_end]
                        region_center = torch.tensor(
                            [
                                (region_box[0] + region_box[2]) * 0.5,
                                (region_box[1] + region_box[3]) * 0.5,
                            ]
                        )
                        region_width = (region_box[2] - region_box[0]).item()
                        region_height = (region_box[3] - region_box[1]).item()
                        init_pos = np.random.normal(
                            loc=region_center.cpu(),
                            scale=[region_width * 0.001, region_height * 0.001],
                            size=[len(region_inst_ids), 2]
                        )
                        pos[region_inst_ids.long()] = torch.from_numpy(init_pos).to(pos.device).to(pos.dtype)

                # fillers
                filler_range = data_cls.filler_range
                filler_bgn = filler_range[0]
                for area_type in range(data_cls.num_area_types):
                    filler_end = filler_bgn + data_cls.num_fillers[area_type].item()
                    # probability is associated with the bin capacity
                    probs = np.array(placedb.binCapacityMap(area_type).tolist())
                    # normalize probability
                    probs /= probs.sum()
                    target_bin_ids = np.random.choice(
                        len(probs), size=filler_end - filler_bgn, p=probs
                    )
                    bin_map_dim = data_cls.bin_map_dims[area_type]
                    bin_map_size = data_cls.bin_map_sizes[area_type]
                    init_pos = np.stack(
                        [
                            (target_bin_ids // bin_map_dim[1]) * bin_map_size[0],
                            (target_bin_ids % bin_map_dim[1]) * bin_map_size[1],
                        ],
                        axis=-1,
                    )
                    offset_x = np.random.uniform(
                        low=0, high=bin_map_size[0], size=filler_end - filler_bgn
                    )
                    offset_y = np.random.uniform(
                        low=0, high=bin_map_size[1], size=filler_end - filler_bgn
                    )
                    init_pos[:, 0] += offset_x
                    init_pos[:, 1] += offset_y
                    pos[filler_bgn:filler_end] = torch.from_numpy(init_pos)

                    filler_bgn = filler_end

                # move inside boundary
                self.move_boundary_op(pos)

        return random_pos

    def build_hpwl_op(self, params, placedb, data_cls):
        """Half-perimeter wirelength"""
        op = hpwl.HPWL(
            flat_netpin=data_cls.net_pin_map.bs,
            netpin_start=data_cls.net_pin_map.b_starts,
            net_weights=data_cls.net_weights,
            net_mask=data_cls.net_mask,
        )

        def hpwl_op(pos):
            pin_pos = self.pin_pos_op(pos)
            return op(pin_pos)

        return hpwl_op

    def build_wawl_op(self, params, placedb, data_cls):
        """Weighted-average wirelength"""
        op = wawl.WAWL(
            flat_netpin=data_cls.net_pin_map.bs,
            netpin_start=data_cls.net_pin_map.b_starts,
            pin2net_map=data_cls.net_pin_map.b2as,
            net_weights=data_cls.net_weights,
            # net_mask=data_cls.net_mask,
            net_mask=data_cls.net_mask_ignore_large,
            pin_mask=data_cls.pin_mask,
            gamma=data_cls.gamma.gamma,
        )

        def wawl_op(pos):
            pin_pos = self.pin_pos_op(pos)
            return op(pin_pos)

        return wawl_op

    def build_electric_overflow_op(self, params, placedb, data_cls):
        """Electric overflow
        This area, not ratio
        """
        return electric_potential.ElectricOverflow(
            inst_sizes=data_cls.inst_sizes,
            # inst_area_types=data_cls.inst_area_types,
            initial_density_maps=data_cls.initial_density_maps,
            bin_map_dims=torch.from_numpy(data_cls.bin_map_dims).to(
                data_cls.inst_sizes.device
            ),
            area_type_mask=data_cls.area_type_mask,
            xl=data_cls.diearea[0],
            yl=data_cls.diearea[1],
            xh=data_cls.diearea[2],
            yh=data_cls.diearea[3],
            movable_range=data_cls.movable_range,
            filler_range=data_cls.filler_range,
            fixed_range=data_cls.fixed_range,
            target_density=data_cls.target_density,
            smooth_flag=False,
            deterministic_flag=params.deterministic_flag,
            movable_macro_mask=None,
        )

    def build_normalized_overflow_op(self, params, placedb, data_cls):
        """Normalize overflow
        This is ratio, not area
        """

        def normalized_overflow_op(pos):
            return self.stable_zero_div_op(
                self.overflow_op(pos), data_cls.total_movable_areas
            )

        return normalized_overflow_op
