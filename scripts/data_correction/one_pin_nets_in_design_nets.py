#!/usr/bin/env python
# -*- coding:utf-8 -*-
###
# @file          : clean_mlcad.py
# @project       : scripts
# @author        : Jing Mai <jingmai@pku.edu.cn>
# @created date  : July 07 2023, 17:05:34, Friday
# @brief         :
# -----
# Last Modified: August 23 2023, 18:18:45, Wednesday
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

# %%
import os
import os.path as osp

# %%
current_directory = os.getcwd()
subdirectories = next(os.walk(current_directory))[1]

# remove nets with only one pin
# %%
for subdir in subdirectories:
    file_path = osp.join(current_directory, subdir, "design.nets")
    bk_path = osp.join(current_directory, subdir, "design.nets.one_pin_net.bk")
    print("Processing %s" % file_path)

    if not osp.exists(file_path):
        continue

    os.system("cp {0} {1}".format(file_path, bk_path))

    with open(file_path, "r") as f:
        lines = f.readlines()

    of = open(file_path, "w")
    cur_net = None
    for line in lines:
        if line.startswith("net"):
            cur_net = [line]
        elif line.startswith("endnet"):
            cur_net.append(line)
            if len(cur_net) <= 3:
                # print("Net %s removed" % cur_net[0].split()[1])
                pass
            else:
                for l in cur_net:
                    of.write(l)
        else:
            if cur_net is not None:
                cur_net.append(line)
            else:
                of.write(line)
