#!/usr/bin/env bash

function handle_ctrl_c {
    echo "Ctrl-C detected. Exiting ..."
    exit 1
}

trap handle_ctrl_c SIGINT

if [ $# -ne 4 ]; then
    echo "Usage: $0 <source_dir> <benchmark_dir> <config_path> <home_path>"
    exit 1
fi

source_dir=$1
benchmark_dir=$2
config_path=$3
home_path=$4
current_dir="$(pwd)"

host=$(hostname)
date_str="${host}_$(date +"%Y.%m.%d_%H.%M.%S")"
run_dir="../mlcad2023_results/run_${date_str}"

echo "sourcr dir            : $source_dir"
echo "benchmark dir         : $benchmark_dir"
echo "config path           : $config_path"
echo "home path             : $home_path"
echo "current dir           : $current_dir"
echo "run dir               : $run_dir"

echo ""
read -p "Description: "
desp=$REPLY
echo "desp: $desp"

echo "Git stash current change with name: ${date_str}"
cd ${source_dir}
git stash save "Regression: ${date_str}" --include-untracked && git stash apply --quiet
cd ${current_dir}

mkdir -p "${run_dir}"
echo $desp > "${run_dir}/description.txt"

echo "${source_dir}/scripts/mlcad2023_place.sh ${source_dir} ${benchmark_dir} ${config_path} ${run_dir}"
${source_dir}/scripts/mlcad2023_place.sh ${source_dir} ${benchmark_dir} ${config_path} ${run_dir}

echo "python ${source_dir}/scripts/mlcad2023_bookshelf2vivado.py --benchmark_dir ${benchmark_dir} --run_dir ${run_dir}"
python ${source_dir}/scripts/mlcad2023_bookshelf2vivado.py --benchmark_dir ${benchmark_dir} --run_dir ${run_dir}

echo "python ${source_dir}/scripts/mlcad2023_vivado.py --home_path $home_path --vivado_bin $(which vivado) --benchmark_dir ${benchmark_dir} --run_dir ${run_dir}"
python ${source_dir}/scripts/mlcad2023_vivado.py --home_path $home_path --vivado_bin $(which vivado) --benchmark_dir ${benchmark_dir} --run_dir ${run_dir}

echo "python ${source_dir}/scripts/mlcad2023_eval.py --benchmark_dir ${benchmark_dir} --run_dir ${run_dir}"
python ${source_dir}/scripts/mlcad2023_eval.py --benchmark_dir ${benchmark_dir} --run_dir ${run_dir}

