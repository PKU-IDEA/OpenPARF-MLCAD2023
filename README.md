# OpenPARF-MLCAD2023

This code is the submission from team MPKU for the MLCAD 2023 FPGA Macro Placement contest. [This code ranked first among all submitted teams](https://ieeexplore.ieee.org/document/10299868) after correcting data errors.

## Table of Contents

* [Data Correction](#data-correction)
* [Macro Placement](#macro-placement)
    - [Using Pre-built Docker Image](#using-pre-built-docker-image)
    - [Build from Source](#build-from-source)
* [Bookshelf to Vivado TCL Conversion](#bookshelf-to-vivado-tcl-conversion)
* [One-click Run and Evaluation Script (Extremely Useful!)](#-one-click-run-and-evaluation-script-extremely-useful)

## Data Correction

Before we start, we would like to point out that we have made some corrections to [the updated benchmark suite](https://www.kaggle.com/datasets/ismailbustany/updated-mlcad-2023-contest-benchmark) provided by the organizers. In addition to the problems mentioned in the FAQ, we have also encountered other problems.

> [! CAUTION]
> It has come to our attention that it is imperative to rectify this data in order to ensure the seamless operation of the program.

For the convenience of the organizers, we have provided scripts to correct the benchmark. These scripts are located in the `scripts/data_correction` folder. To correct the data, please follow the steps below:

```bash
cd <path to the benchmark suite>
bash <source dir>/scripts/data_correction/fix_benchmark.sh <source dir>
```

## Macro Placement

You can either use our pre-built docker image or build from source.

### Using Pre-built Docker Image

Download the docker image from Docker Hub:

```bash
docker pull magic3007/openparf:mlcad2023-final
```

After that, you can start a container like this:

```bash
docker run -it --rm \
    -v <benchmark directory on host>:/root/benchmarks/mlcad2023 \
    -v <output result directory on host>:/root/OpenPARF/mlcad2023_results \
    magic3007/openparf:mlcad2023-final
```

You will then be present with a bash shell inside the container.
The container includes our source code and precompiled binary under `/root/OpenPARF` .
You can use the following command to place one single design (e.g., `Design_2` ):

```bash
cd /root/OpenPARF/install
python openparf.py \
    --config unittest/regression/mlcad2023/mlcad2023.cpu.json \
    --benchmark_name Design_2 \
    --input_dir /root/benchmarks/mlcad2023/Design_2 \
    --result_dir result_Design_2 2>&1 | tee Design_2.log
```

We provide a batch script to run all benchmarks, which you can use like this:

```bash
cd /root/OpenPARF/install
bash ../scripts/mlcad2023_place.sh /root/OpenPARF ~/benchmarks/mlcad2023 unittest/regression/mlcad2023/mlcad2023.cpu.json /root/OpenPARF/mlcad2023_results
```

The results are stored in `/root/OpenPARF/mlcad2023_results/` within the container, which is mounted to `<output result directory on host>` on the host machine.

### Build from Source

You can reproduce our build environment by looking at the Dockerfile at `docker/openparf.dockerfile` .
It contains the installation instructions of all dependencies.
To build a docker environment, you can use the following commands:

```bash
cd docker
docker build . -t openparf:mlcad2023-v1.0 -f ./openparf.dockerfile
```

After the environment is ready, you can enter the environment using the following command.
You need to fill in the project directory and the benchmark directory so they can be mounted inside the container.

```bash
docker run -it --rm \
  -v <project directory on host>:/root/OpenPARF \
  -v <benchmark directory on host>:/root/benchmarks/mlcad2023 \
  -v <output result directory on host>:/root/mlcad2023_results \
  openparf:mlcad2023-v1.0
```

Finally within the container, you can use the following commands to compile the project:

```bash
cd /root/OpenPARF
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/conda -DPYTHON_EXECUTABLE=$(which python) -DPython3_EXECUTABLE=$(which python) -DCMAKE_INSTALL_PREFIX=../install -DENABLE_ROUTER=OFF
make -j20
make install
```

When the compilation succeeds, it is ready to be run on any benchmark, or run all benchmarks in a batch, just like in our prebuilt container.

```bash
# ad-hoc run, e.g., Design_2
cd /root/OpenPARF/install
python openparf.py \
    --config unittest/regression/mlcad2023/mlcad2023.cpu.json \
    --benchmark_name Design_2 \
    --input_dir /root/benchmarks/mlcad2023/Design_2 \
    --result_dir result_Design_2 2>&1 | tee Design_2.log

# batch run, the results are located at /root/mlcad2023_results
cd /root/OpenPARF/install
bash ../scripts/mlcad2023_place.sh /root/OpenPARF ~/benchmarks/mlcad2023 unittest/regression/mlcad2023/mlcad2023.cpu.json /root/OpenPARF/mlcad2023_results
```

The results are stored in `/root/OpenPARF/mlcad2023_results/` within the container, which is mounted to `<output result directory on host>` on the host machine.

## Bookshelf to Vivado TCL Conversion

After the macro placement, we need to convert the `pl` file to a `tcl` script for the next step. We provide a script to do this. You can use it like this:

```bash
# ad-hoc run, e.g., Design_2
# the output tcl file `place_macro.tcl` is located at <output result directory on host>/solutions/Design_2/place_macro.tcl
python <source dir>/scripts/macroplacement_bookshelf2vivado.py \
    --bm_dir <benchmark directory on host>/Design_2 \
    --sol_dir <output result directory on host>/solutions/Design_2
```

## 🌟 One-click Run and Evaluation Script (Extremely Useful!)

In the final MLCAD2023 evaluation, Vivado ML 2021.1 is used to complete the standard cell placement. Specifically, the entire evaluation process is divided into the following steps:
1. Custom placer completes macro placement (corresponding to the [Macro Placement](#macro-placement) section and our [mlcad2023_place.sh](./scripts/mlcad2023_place.sh) script)
2. Convert the pl file to a Vivado tcl script (corresponding to the [Bookshelf to Vivado TCL Conversion](#bookshelf-to-vivado-tcl-conversion) section and our [mlcad2023_bookshelf2vivado.py](./scripts/mlcad2023_bookshelf2vivado.py) script)
3. Use Vivado to complete standard cell placement and routing (corresponding to our [mlcad2023_vivado.py](./scripts/mlcad2023_vivado.py) script)
4. Use Vivado to calculate evaluation metrics (corresponding to our [mlcad2023_eval.py](./scripts/mlcad2023_eval.py) script)

We provide a complete script [mlcad2023_one4all.sh](./scripts/mlcad2023_one4all.sh) to run these four steps, which you can use with the following command:

```bash
bash <source dir>/scripts/mlcad2023_one4all.sh <source dir> <benchmark dir> <config path> <home path>
# Inside the docker container, you can use the following command to run
bash ../scripts/mlcad2023_one4all.sh /root/OpenPARF ~/benchmarks/mlcad2023 unittest/regression/mlcad2023/mlcad2023.cpu.json /root/OpenPARF/mlcad2023_results
```

## The Team

The main contributors to this repo are from the [PKU-IDEA](https://github.com/PKU-IDEA) Lab at Peking University, advised by [Prof. Yibo Lin](https://yibolin.com). The collaborators include:
* [Jing Mai](https://magic3007.github.io), [Jiarui Wang](https://tomjerry213.github.io), Yifan Chen, [Zizheng Guo](https://guozz.cn), Xun Jiang, and [Yibo Lin](https://yibolin.com)

## Citation

If you find our work useful, please consider citing us as stated in the [main repo of OpenPARF](https://github.com/PKU-IDEA/OpenPARF).

## License

This software is released under BSD 3-Clause License, Please refer to [LICENSE](./LICENSE) for details.
