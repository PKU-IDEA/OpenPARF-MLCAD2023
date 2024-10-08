#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# File              : net_density.py
# Author            : Yifan Chen <chenyifan2019@pku.edu.cn>
# Date              : 08.19.2023
# Last Modified Date: 08.19.2023
# Last Modified By  : Yifan Chen <chenyifan2019@pku.edu.cn>

import torch
from torch import nn

from openparf import configure
from . import net_density_cpp

if configure.compile_configurations["CUDA_FOUND"] == "TRUE":
    from . import net_density_cuda


class NetDensity(nn.Module):
    def __init__(self,
                 netpin_start,
                 flat_netpin,
                 net_weights,
                 xl,
                 xh,
                 yl,
                 yh,
                 num_bins_x, num_bins_y,
                 deterministic_flag,
                 initial_horizontal_net_density_map=None,
                 initial_vertical_net_density_map=None):
        """ Constructor of NetDensity/RISA operator.

        :param netpin_start: starting index in netpin map for each net, length of #nets+1, the last entry is #pins
        :param flat_netpin: flat netpin map, length of #pins
        :param net_weights: weight of nets, length of #nets
        :param xl: minimum x-coordinates of the layout
        :param xh: maximum x-coordinates of the layout
        :param yl: minimum y-coordinates of the layout
        :param yh: maximum y-coordinates of the layout
        :param num_bins_x: number of bins in the x-axis direction
        :param num_bins_y: number of bins in the y-axis direction
        :param initial_horizontal_net_density_map: initial horizontal net density map, length of num_bins_x * num_bins_y
        :param initial_vertical_net_density_map initial vertical net density map, length of num_bins_x * num_bins_y
        """
        super(NetDensity, self).__init__()
        self.netpin_start = netpin_start
        self.flat_netpin = flat_netpin
        self.net_weights = net_weights
        self.xl = xl
        self.yl = yl
        self.xh = xh
        self.yh = yh
        self.num_bins_x = num_bins_x
        self.num_bins_y = num_bins_y
        self.bin_size_x = (xh - xl) / num_bins_x
        self.bin_size_y = (yh - yl) / num_bins_y
        self.determistic_flag = deterministic_flag
        self.initial_horizontal_net_density_map = initial_horizontal_net_density_map
        self.initial_vertical_net_density_map = initial_vertical_net_density_map

    def forward(self, pin_pos):
        """ Forward function that calculates the routing congestion map

        :param pin_pos: tensor of pin position, length of 2 * #pins, in the form of xyxyxy...
        :return: net density map, H & V net density map
        """
        horizontal_net_density_map = torch.zeros((self.num_bins_x, self.num_bins_y),
                                                 dtype=pin_pos.dtype,
                                                 device=pin_pos.device)
        vertical_net_density_map = torch.zeros_like(horizontal_net_density_map)
        foo = net_density_cuda.forward if pin_pos.is_cuda else net_density_cpp.forward
        foo(pin_pos,
            self.netpin_start,
            self.flat_netpin,
            self.net_weights,
            self.bin_size_x,
            self.bin_size_y,
            self.xl,
            self.yl,
            self.xh,
            self.yh,
            self.num_bins_x,
            self.num_bins_y,
            self.determistic_flag,
            horizontal_net_density_map,
            vertical_net_density_map)

        if self.initial_horizontal_net_density_map is not None:
            horizontal_net_density_map.add_(self.initial_horizontal_net_density_map)
        if self.initial_vertical_net_density_map is not None:
            vertical_net_density_map.add_(self.initial_vertical_net_density_map)

        net_density_map = torch.add(horizontal_net_density_map.abs(), vertical_net_density_map.abs())

        # Routing Utilization Overflow
        return net_density_map, horizontal_net_density_map, vertical_net_density_map
