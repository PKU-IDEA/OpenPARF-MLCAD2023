#!/usr/bin/env bash

function handle_ctrl_c {
    echo "Ctrl-C detected. Exiting ..."
    exit 1
}

trap handle_ctrl_c SIGINT

if [ $# -ne 4 ]; then
    echo "Usage: $0 <source_dir> <benchmark_dir> <config_path> <run_dir>"
    exit 1
fi

source_dir=$1
benchmark_dir=$2
config_path=$3
run_dir=$4

if [ ! -d "${benchmark_dir}" ]; then
    echo "directory \"${benchmark_dir}\" does NOT exist"
    exit 1
fi

if [ ! -f "${config_path}" ]; then
    echo "File \"${config_path}\" does NOT exist"
    exit 1
fi

log_dir="${run_dir}/logs"
solution_dir="${run_dir}/solutions"
plot_dir="${run_dir}/plots"

# collect the directories under design_dir
design_dirs=($(find "${benchmark_dir}" -maxdepth 1 -mindepth 1 -type d -follow))
design_dirs=($(printf "%s\n" "${design_dirs[@]}" | sort -V))
num_designs=${#design_dirs[@]}

echo ""
echo "benchmark_dir : ${benchmark_dir}"
echo "#designs      : ${num_designs}"
echo "run_dir       : ${run_dir}"
echo "log_dir       : ${log_dir}"
echo "solution_dir  : ${solution_dir}"
echo "plot_dir      : ${plot_dir}"

mkdir -p "${log_dir}"
mkdir -p "${solution_dir}"
mkdir -p "${plot_dir}"


for ((i=0; i <num_designs; i++))
do
    design_dir=${design_dirs[$i]}
    design_name=$(basename "$design_dir")

    progress=$(( (i+1) * 100 / num_designs ))
    bar="["
    for ((j=0; j<progress/5; j++))
    do
        bar+="="
    done
    bar+=">"
    for ((j=progress/5+1; j<20; j++))
    do
        bar+=" "
    done
    bar+="]"

    echo -e "\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
    echo -e "Processing $design_name ... $progress% $bar\r"

    screen_log_file_path="${log_dir}/${design_name}.log"
    solution_design_dir="${solution_dir}/${design_name}"
    plot_design_dir="${plot_dir}/${design_name}"

    mkdir -p ${solution_design_dir}
    mkdir -p ${plot_design_dir}

    echo "config_path           : ${config_path}"
    echo "design_dir            : ${design_dir}"
    echo "screen_log_file_path  : ${screen_log_file_path}"
    echo "solution_design_dir   : ${solution_design_dir}"
    echo "plot_design_dir       : ${plot_design_dir}"

    echo "python openparf.py --config ${config_path} --benchmark_name ${design_name} --input_dir ${design_dir} --result_dir ${solution_design_dir} --plot_dir ${plot_design_dir} 2>&1 | tee ${screen_log_file_path}"
    start_time=$(date +%s)
    python openparf.py --config ${config_path} --benchmark_name ${design_name} --input_dir ${design_dir} --result_dir ${solution_design_dir} --plot_dir ${plot_design_dir} 2>&1 | tee ${screen_log_file_path}
	if [ $? -eq 0 ]; then
        echo "${design_name} runs successfully."
	else
        echo "${design_name} fails."
    fi
    end_time=$(date +%s)
    runtime=$((end_time - start_time))
    echo "Total Runtime: $runtime seconds" >> ${screen_log_file_path}
done

echo -e "\nAll designs processed."
