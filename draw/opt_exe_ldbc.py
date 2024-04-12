import numpy as np
import matplotlib.pyplot as plt

queries = ['IC[1-3]', 'IC[2]', 'IC[4]', 'IC[7]']
methods = ['RelGo', 'Calcite', 'GrainDB']

compile_times = np.array([
    [91, 15, 60, 26], # compilation time of RelGo
    [10201, 394, 25839, 60000], # Calcite
    [1.04, 0.69, 1.61, 0.96], # GrainDB
])

execution_times = np.array([
    [376.78, 374.47, 64.15, 39.51], # execution time of RelGo
    [259.75, 11217.25, 1398.20, 1], # Calcite, ic1-3 may not good enough; 1 is a placeholder
    [1073.82, 395.16, 286.14, 1172.55], # GrainDB
])


plt.figure(figsize=(10, 6))

n_queries = len(queries)
n_methods = len(methods)

group_width = 0.8
bar_width = group_width / n_methods
index = np.arange(n_queries)

compile_colors = ['#2ca02c', '#ff7f0e', '#1f77b4']  # Green, Orange, Blue
execution_colors = ['#98df8a', '#ffbb78', '#aec7e8']  # Lighter Green, Lighter Orange, Lighter Blue

for i in range(n_methods):
    plt.bar(index + i * bar_width, compile_times[i], bar_width, color=compile_colors[i], label=f'{methods[i]} Opt. Time Cost', bottom=np.zeros(n_queries))
    plt.bar(index + i * bar_width, execution_times[i], bar_width, color=execution_colors[i], label=f'{methods[i]} Exe. Time Cost', bottom=compile_times[i])

plt.legend()

plt.xticks(index + group_width / 3, queries)

plt.ylabel('Time Cost (ms)')

ot_threshold_ms = 60000
ot_threshold_units = ot_threshold_ms  
plt.axhline(y=ot_threshold_units, color='r', linestyle='--', label=f'OT Threshold ({ot_threshold_ms} ms)')

plt.legend()

plt.yscale('log')
# plt.show()
plt.savefig('../paper/figures/exp/opt_exe_ldbc.pdf', bbox_inches='tight')