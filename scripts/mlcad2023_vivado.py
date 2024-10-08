#!/usr/bin/env python
# -*- coding:utf-8 -*-
###
# @file          : mlcad2023_route.py
# @project       : OpenPARF
# @author        : Jing Mai <jingmai@pku.edu.cn>
# @created date  : July 19 2023, 20:02:38, Wednesday
# @brief         :
# -----
# Last Modified: August 21 2023, 14:12:47, Monday
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
import concurrent.futures
import subprocess
import pathlib
import time
import os
import threading
import random
import os.path as osp
import argparse
import pandas as pd

# %%
# [IN]
parser = argparse.ArgumentParser()
parser.add_argument("--home_path", type=str, required=True)
parser.add_argument("--vivado_bin", type=str, required=True)
parser.add_argument("--benchmark_dir", type=str, required=True)
parser.add_argument("--run_dir", type=str, required=True)
parser.add_argument("--timeout_second", type=int, default=3600 * 5) # if PnR takes more than 5 hours, kill it
parser.add_argument("--tmp_dir", type=str, default="/tmp")
parser.add_argument("--max_workers", type=int, default=6)

args = parser.parse_args()
home_path = args.home_path
vivado_bin = args.vivado_bin
benchmark_dir = args.benchmark_dir
run_dir = args.run_dir
timeout_second = args.timeout_second
tmp_dir = args.tmp_dir
max_workers = args.max_workers

# %%
assert osp.exists(run_dir), "run_dir does not exist"
benchmark_names = [d.name for d in os.scandir(benchmark_dir) if d.is_dir()]
benchmark_names = sorted(benchmark_names, key=lambda x: int(''.join(filter(str.isdigit, x))))
vivado_solution_dir = os.path.join(run_dir, "vivado_solutions")
result_dir = os.path.join(run_dir, "vivado_results")
placed_checkpoint_dir = os.path.join(result_dir, "placed_checkpoints")
routed_checkpoint_dir = os.path.join(result_dir, "routed_checkpoints")
log_dir = os.path.join(result_dir, "log")
assert osp.exists(vivado_solution_dir), "vivado_solution_dir does not exist"
pathlib.Path(placed_checkpoint_dir).mkdir(parents=True, exist_ok=True)
pathlib.Path(routed_checkpoint_dir).mkdir(parents=True, exist_ok=True)
pathlib.Path(log_dir).mkdir(parents=True, exist_ok=True)

# %%
print("home_path            : ", home_path)
print("vivado_bin           : ", vivado_bin)
print("benchmark_dir        : ", benchmark_dir)
print("run_dir              : ", run_dir)
print("vivado_solution_dir  : ", vivado_solution_dir)
print("log_dir              : ", log_dir)
print("placed_dcp_dir       : ", placed_checkpoint_dir)
print("routed_dcp_dir       : ", routed_checkpoint_dir)
print("# benchmarks         : ", len(benchmark_names))

# %%
running = set()
running_subprocesses = set()

futures_map = dict()
finished_fp = open(os.path.join(result_dir, "finished.txt"), "w")
lck = threading.Lock()

# %%
tcl_script_content = """

proc launch {
    benchmark_name
    dcp_path
    io_tcl_path
    macro_tcl_path
    placed_checkpoint_path
    routed_checkpoint_path
} {
    puts "=============== $benchmark_name  =================="

    # Start measuring the runtime
    set start_time [clock seconds]

    if {[file exists $routed_checkpoint_path] == 0} {
        puts "Opening Unrouted Checkpoint..."
        open_checkpoint $dcp_path

        set_param place.timingDriven false

        puts "Automatic IO Placement..."
        source $io_tcl_path

        puts "Placing DSPs and BRAMs..."
        source $macro_tcl_path

        puts "Placing Design..."
        place_design -verbose

        # puts "Writing Placed Checkpoints..."
        # write_checkpoint -force $placed_checkpoint_path

        puts "Routing Design..."
        route_design -no_timing_driven -verbose

        # puts "Writing Routed Checkpoints..."
        # write_checkpoint -force $routed_checkpoint_path
    } else {
        puts "Routed Checkpoint..."
        open_checkpoint $routed_checkpoint_path
    }

    puts "Report Congestiong Level..."
    report_design_analysis -congestion -min_congestion_level 3

    puts "Reporting Routing Status..."
    report_route_status

    # End measuring the runtime
    set end_time [clock seconds]

    # Calculate the elapsed time
    set elapsed_time [expr {$end_time - $start_time}]
    puts "Vivado Total Elapsed Time: $elapsed_time seconds"

    puts "Closing Project..."
    close_project

    puts "$benchmark_name: All done..."
}

if { $argc != 6 } {
    puts "The script requires 6 arguments."
    puts "Please try again."
} else {
    set benchmark_name [lindex $argv 0]
    set dcp_path [lindex $argv 1]
    set io_tcl_path [lindex $argv 2]
    set macro_tcl_path [lindex $argv 3]
    set placed_checkpoint_path [lindex $argv 4]
    set routed_checkpoint_path [lindex $argv 5]
    puts $benchmark_name
    puts $dcp_path
    puts $io_tcl_path
    puts $macro_tcl_path
    puts $placed_checkpoint_path
    puts $routed_checkpoint_path
    launch $benchmark_name $dcp_path $io_tcl_path $macro_tcl_path $placed_checkpoint_path $routed_checkpoint_path
}
"""

# %%


def route(
    benchmark_name,
    dcp_path,
    io_tcl_path,
    macro_tcl_path,
    placed_checkpoint_path,
    routed_checkpoint_path,
    log_path,
    lck,
):
    print("Started Running {0}......".format(benchmark_name))

    lck.acquire()
    running.add(benchmark_name)
    lck.release()

    tt = time.time()

    tmp_tcl_file = (
        tmp_dir + "/" + str(random.random())[2:] + "_" + str(benchmark_name) + ".tcl"
    )
    with open(tmp_tcl_file, "w") as fp:
        fp.write(tcl_script_content)

    args = [
        vivado_bin,
        "-mode",
        "batch",
        "-source",
        tmp_tcl_file,
        "-log",
        log_path,
        "-tclargs",
        benchmark_name,
        dcp_path,
        io_tcl_path,
        macro_tcl_path,
        placed_checkpoint_path,
        routed_checkpoint_path,
    ]
    proc = subprocess.Popen(
        args,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        env={"HOME": home_path},
    )
    running_subprocesses.add(proc)

    try:
        proc.wait(timeout=timeout_second)
    except subprocess.TimeoutExpired as e:
        print("{0} Timed out!".format(benchmark_name))
        proc.kill()
    finally:
        elapsed_time = time.time() - tt
        finished_fp.write(
            "Elapsed time of {0}: {1} min\n".format(benchmark_name, elapsed_time / 60)
        )
    if proc.returncode != 0:
        print("{0} returned with {1}".format(benchmark_name, proc.returncode))

    lck.acquire()
    if benchmark_name in running:
        running.remove(benchmark_name)
    lck.release()

    running_subprocesses.remove(proc)


# %%
with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
    try:
        for benchmark_name in benchmark_names:
            dcp_path = os.path.join(benchmark_dir, benchmark_name, "design.dcp")
            io_tcl_path = osp.join(benchmark_dir, benchmark_name, "place_io.tcl")
            place_csv_path = os.path.join(
                vivado_solution_dir, benchmark_name, "macroplacement.vivado.csv"
            )
            log_path = os.path.join(log_dir, "{0}.log".format(benchmark_name))
            placed_checkpoint_path = os.path.join(
                placed_checkpoint_dir, benchmark_name, "placed_design.dcp"
            )
            routed_checkpoint_path = os.path.join(
                routed_checkpoint_dir, benchmark_name, "routed_design.dcp"
            )

            if not pathlib.Path(place_csv_path).is_file():
                print("No placement file for {0}".format(benchmark_name))
                finished_fp.write("No placement file for {0}\n".format(benchmark_name))
                continue

            pathlib.Path(os.path.dirname(placed_checkpoint_path)).mkdir(
                parents=True, exist_ok=True
            )
            pathlib.Path(os.path.dirname(routed_checkpoint_path)).mkdir(
                parents=True, exist_ok=True
            )
            assert pathlib.Path(dcp_path).is_file()
            assert pathlib.Path(io_tcl_path).is_file()

            df = pd.read_csv(place_csv_path)
            macro_tcl_path = osp.join(
                tmp_dir,
                "place_"
                + str(random.random())[2:]
                + "_"
                + str(benchmark_name)
                + ".tcl",
            )
            with open(macro_tcl_path, "w") as fp:
                fp.write("place_cell { \\\n")
                for i, row in df.iterrows():
                    fp.write("  {0} {1} \\\n".format(row["NAME"], row["LOC"]))
                fp.write("}\n")

            future = executor.submit(
                route,
                benchmark_name,
                dcp_path,
                io_tcl_path,
                macro_tcl_path,
                placed_checkpoint_path,
                routed_checkpoint_path,
                log_path,
                lck,
            )
            futures_map[future] = benchmark_name

        start_time = time.time()
        print("Pending", executor._work_queue.qsize(), "tasks")
        print("Currently running", running)
        for future in concurrent.futures.as_completed(futures_map):
            benchmark_name = futures_map[future]
            st = "{0} finished after {1:.1f} min.".format(
                benchmark_name, (time.time() - start_time) / 60.0
            )
            print(st)
            finished_fp.write(st + "\n")
            print("Pending", executor._work_queue.qsize(), "tasks")
            print("Currently running", running)
    except KeyboardInterrupt:
        print("Killing running subprocesses")
        for p in running_subprocesses:
            p.kill()
        print("Cancelling pending futures")
        for f in futures_map:
            f.cancel()

# %%
finished_fp.close()
