# %%
import os
import argparse
import eval
import os.path as osp
import pandas as pd

# %%
parser = argparse.ArgumentParser()
parser.add_argument('--run_dir', type=str, required=True)
parser.add_argument('--benchmark_dir', type=str, required=True)
args = parser.parse_args()
run_dir = args.run_dir
benchmark_dir = args.benchmark_dir


# %%
vivado_results_dir = osp.join(run_dir, "vivado_results")
openparf_logs_dir = osp.join(run_dir, "logs")
vivado_logs_dir = osp.join(vivado_results_dir, "log")
eval_csv_path = osp.join(run_dir, osp.basename(run_dir) + ".csv")

print("run_dir              : ", run_dir)
print("vivado_results_dir   : ", vivado_results_dir)
print("openparf_logs_dir    : ", openparf_logs_dir)
print("vivado_logs_dir      : ", vivado_logs_dir)

# %%
fields = [
    "Name",
    "shortCongLevel_N",
    "shortCongLevel_S",
    "shortCongLevel_E",
    "shortCongLevel_W",
    "globalCongLevel_N",
    "globalCongLevel_S",
    "globalCongLevel_E",
    "globalCongLevel_W",
    "phase4Iters",
    "macroplaceGenerationRT",
    "vivadoRouteRT",
    "totalRT",
    "medianRT",
    "initialRoutingCongestionScore",
    "routingCongestionScore",
    "MacroRTScore",
    "RuntimeFactor",
    "FinalScore",
]

benchmark_names = [d.name for d in os.scandir(benchmark_dir) if d.is_dir()]
benchmark_names = sorted(
    benchmark_names, key=lambda x: int("".join(filter(str.isdigit, x)))
)

df = pd.DataFrame(columns=fields)
for benchmark_name in benchmark_names:
    print("Processing benchmark: ", benchmark_name)
    vivado_log_dir = osp.join(vivado_logs_dir, benchmark_name + ".log")
    if not osp.exists(vivado_log_dir):
        print("Vivado log file not found: ", vivado_log_dir)
        continue
    eval_result = eval.eval(vivado_log_dir)
    df = df.append(
        {
            "Name": int("".join(filter(str.isdigit, benchmark_name))),
            "shortCongLevel_N": eval_result.shortCongestionLevel[0],
            "shortCongLevel_S": eval_result.shortCongestionLevel[1],
            "shortCongLevel_E": eval_result.shortCongestionLevel[2],
            "shortCongLevel_W": eval_result.shortCongestionLevel[3],
            "globalCongLevel_N": eval_result.globalCongestionLevel[0],
            "globalCongLevel_S": eval_result.globalCongestionLevel[1],
            "globalCongLevel_E": eval_result.globalCongestionLevel[2],
            "globalCongLevel_W": eval_result.globalCongestionLevel[3],
            "phase4Iters": eval_result.phase4Iterations,
            "macroplaceGenerationRT": eval_result.macroplacmentGenerationRuntime,
            "vivadoRouteRT": eval_result.VivadoRuntime,
            "totalRT": eval_result.totalRuntime(),
            "medianRT": eval_result.medianTotalRuntime,
            "initialRoutingCongestionScore": eval_result.initialRoutingCongestionScore(),
            "routingCongestionScore": eval_result.routingCongestionScore(),
            "MacroRTScore": eval_result.macroplacementGenerationRuntimeScore(),
            "RuntimeFactor": eval_result.runtimeFactor(),
            "FinalScore": eval_result.finalScore(),
        },
        ignore_index=True,
    )
#%%
print("Saving eval results to: ", osp.realpath(eval_csv_path))
df.to_csv(eval_csv_path, index=False)