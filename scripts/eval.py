#%%
import re
from typing import List
from math import log2
from dataclasses import dataclass, field
import sys
from datetime import datetime

@dataclass
class evalMetrics:
    shortCongestionLevel:  List[int] = field(default_factory=list)
    globalCongestionLevel:  List[int] = field(default_factory=list)
    phase4Iterations: int = 0

    macroplacmentGenerationRuntime: float = 0
    VivadoRuntime: float = 0
    medianTotalRuntime: float = 0


    def initialRoutingCongestionScore(self) -> float:
        return 1.2 * sum([max(0, i-3)**2 for i in self.shortCongestionLevel]) + sum([max(0, i-3)**2 for i in self.globalCongestionLevel])

    def totalRuntime(self) -> float:
        return self.macroplacmentGenerationRuntime + self.VivadoRuntime

    def runtimeFactor(self) -> float:
        return (self.totalRuntime() - self.medianTotalRuntime) / 10

    def routingCongestionScore(self) -> float:
        return self.initialRoutingCongestionScore() * self.phase4Iterations


    def macroplacementGenerationRuntimeScore(self) -> float:
        return 1 + max(0, self.macroplacmentGenerationRuntime - 600) / 60

    def finalScore(self) -> float:
        return self.routingCongestionScore() * self.macroplacementGenerationRuntimeScore() * (1 + self.runtimeFactor())

    def toStr(self) -> str:
        return 'FinalScore={}\tCongestionScore={}\tMacroplaceRuntimScore={}\tRuntimeFactor={}'.format(self.finalScore(), self.routingCongestionScore(), self.macroplacementGenerationRuntimeScore(), self.runtimeFactor())

#%%
def eval(path, mpRuntime=0):
    metrics = evalMetrics()
    metrics.shortCongestionLevel = [0 for i in range(4)]
    metrics.globalCongestionLevel = [0 for i in range(4)]
    metrics.macroplacmentGenerationRuntime = mpRuntime

    getLevel = lambda size: log2(int(size[:size.find('x')]))
    directions = ['NORTH', 'SOUTH', 'EAST', 'WEST']

    with open(path, 'r') as f:
        lines = [line for line in f.readlines()]
        i = 0
        while i < len(lines):
            line = lines[i]
            if line.find('INFO: [Route 35-449] Initial Estimated Congestion') > -1:
                while True:
                    i += 1
                    myline = lines[i]
                    for j, direct in enumerate(directions):
                        if myline.find(direct) > -1:
                            l = re.split('\| *|, *| +', myline)[1:-1]
                            metrics.globalCongestionLevel[j] = getLevel(l[1])
                            metrics.shortCongestionLevel[j] = getLevel(l[5])
                    if myline.find('Congestion Report') > -1:
                        break

            if line.find('Phase 4 Rip-up And Reroute') > -1:
                cnt = 0
                while True:
                    i += 1
                    myline = lines[i]
                    if myline.find('Number of Nodes with overlaps') > -1:
                        cnt += 1
                    if myline.find('Phase 4.1 Global Iteration') and myline.find('Checksum') > -1:
                        break
                metrics.phase4Iterations = cnt

            if line.startswith("# Start of session at"):
                start_time_str = line.split("at:")[-1].strip()
                start_time = datetime.strptime(start_time_str, "%a %b %d %H:%M:%S %Y")

            if line.startswith("INFO: [Common 17-206] Exiting Vivado"):
                end_time_str = line.split("at")[-1].strip().rstrip(".")
                end_time = datetime.strptime(end_time_str, "%a %b %d %H:%M:%S %Y")
            i += 1

    metrics.VivadoRuntime = (end_time - start_time).total_seconds()
    metrics.medianTotalRuntime = metrics.totalRuntime()

    return metrics

#%%
if __name__ == '__main__':
    metrics = eval("/home/jingmai/projects/fpgamacro_workspace/mlcad2023_results/run_Bulbasaur_2023.08.15_23.41.58/vivado_results/log/Design_2.log")
    print(metrics.shortCongestionLevel)
    print(metrics.globalCongestionLevel)
    print(metrics.phase4Iterations)
    print(metrics.VivadoRuntime)
    print(metrics.toStr())

# %%
