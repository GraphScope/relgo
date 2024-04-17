import numpy as np
import matplotlib.pyplot as plt

queries = ["JOB[1]", "JOB[2]", "JOB[3]", "JOB[4]"]
methods = ["RelGo", "GrainDB"]

compile_times = np.array(
    [
        [19, 13, 14, 14],  # compilation time of RelGo
        [1, 1, 1, 1],  # GrainDB
    ]
)

execution_times = np.array(
    [
        [35, 143, 320, 348],  # execution time of RelGo
        [224, 221, 594, 517],  # GrainDB
    ]
)


plt.figure(figsize=(10, 5))

n_queries = len(queries)
n_methods = len(methods)

group_width = 0.6
bar_width = group_width / n_methods
index = np.arange(n_queries)

compile_colors = ["#98df8a", "salmon"]  # Green, Orange, Blue
execution_colors = [
    "#C0EAB9",
    "#FAB3AA",
]  # Lighter Green, Lighter Orange, Lighter Blue

for i in range(n_methods):
    plt.bar(
        index + i * bar_width,
        compile_times[i],
        bar_width,
        color=compile_colors[i],
        edgecolor="k",
        label=f"{methods[i]} Opt. Time Cost",
        bottom=np.zeros(n_queries),
    )
    plt.bar(
        index + i * bar_width,
        execution_times[i],
        bar_width,
        color=execution_colors[i],
        edgecolor="k",
        label=f"{methods[i]} Exe. Time Cost",
        bottom=compile_times[i],
    )

plt.xticks(index + group_width / 4, queries, fontsize=28)
plt.yticks(fontsize=28)

plt.ylabel("Time Cost (ms)", fontsize=28)
plt.grid(linestyle="--", axis="y")

# plt.yscale('log')
# plt.show()
plt.tight_layout()
plt.savefig("../paper/figures/exp/opt_exe_job.pdf", bbox_inches="tight")
