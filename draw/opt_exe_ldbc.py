import numpy as np
import matplotlib.pyplot as plt

queries = ["IC[1-3]", "IC[2]", "IC[4]", "IC[7]"]
methods = ["RelGo", "Calcite", "GrainDB"]

compile_times = np.array(
    [
        [91, 15, 60, 26],  # compilation time of RelGo
        [10201, 394, 25839, 60000],  # Calcite
        [1.04, 0.69, 1.61, 0.96],  # GrainDB
    ]
)

execution_times = np.array(
    [
        [376.78, 374.47, 64.15, 39.51],  # execution time of RelGo
        [
            259.75,
            11217.25,
            1398.20,
            1,
        ],  # Calcite, ic1-3 may not good enough; 1 is a placeholder
        [1073.82, 395.16, 286.14, 1172.55],  # GrainDB
    ]
)


plt.figure(figsize=(10, 5))

n_queries = len(queries)
n_methods = len(methods)

group_width = 0.8
bar_width = group_width / n_methods
index = np.arange(n_queries)

compile_colors = ["#98df8a", "salmon", "lightskyblue"]  # Green, Orange, Blue
execution_colors = [
    "#C0EAB9",
    "#FAB3AA",
    "#BCE9FA",
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

plt.xticks(index + group_width / 3, queries, fontsize=28)
plt.yticks(fontsize=28)

plt.ylabel("Time Cost (ms)", fontsize=28)

ot_threshold_ms = 60000
ot_threshold_units = ot_threshold_ms
plt.axhline(
    y=ot_threshold_units,
    color="r",
    linestyle="--",
    # label=f"OT Threshold ({ot_threshold_ms} ms)",
)
plt.yscale("log")
plt.grid(linestyle="--", axis="y")

plt.legend(
    [plt.Line2D([], [], color="r", linestyle="")],
    [f"OT Threshold ({ot_threshold_ms} ms)"],
    loc="upper center",
    bbox_to_anchor=(0.24, 0.97),
    ncol=1,
    frameon=False,
    handletextpad=0.1,
    columnspacing=0.5,
    prop={"size": 24},
)

plt.tight_layout()
# plt.show()
plt.savefig("../paper/figures/exp/opt_exe_ldbc.pdf", bbox_inches="tight")

labels = []
for method in methods:
    labels.append(f"{method} Opt. Time Cost")
    labels.append(f"{method} Exe. Time Cost")

handles = []
for i in range(len(compile_colors)):
    handles.append(
        plt.Line2D(
            [], [], color=compile_colors[i], marker="s", linestyle="", markersize=20
        )
    )
    handles.append(
        plt.Line2D(
            [], [], color=execution_colors[i], marker="s", linestyle="", markersize=20
        )
    )

fig, ax = plt.subplots(figsize=(15, 1))
ax.axis("off")

ax.legend(
    handles,
    labels,
    loc="upper center",
    bbox_to_anchor=(0.5, 1.4),
    ncol=3,
    frameon=True,
    handletextpad=0.1,
    columnspacing=0.5,
    prop={"size": 22},
)
plt.tight_layout()
# plt.show()
plt.savefig("../paper/figures/exp/opt_exe_legends.pdf", bbox_inches="tight")
