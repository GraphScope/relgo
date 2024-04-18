import matplotlib.pyplot as plt
import numpy as np

xlabel = []

for i in range(1, 11):
    xlabel.append(str(i))

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))

y_duckdb = [379, 424, 1383, 782, 387, 932, 1120, 1349, 2541, 1745]

y_back_to_hash = [329, 288, 1040, 480, 128, 946, 737, 1376, 2337, 1728]

y_graindb = [224, 221, 594, 517, 366, 330, 451, 613, 1082, 714]

y_ours = [35, 143, 320, 348, 49, 53, 107, 455, 770, 620]


total_width, n = 0.8, 4
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.figure(figsize=(15, 4))


plt.bar(
    xaxis,
    y_ours,
    color="lightgreen",
    edgecolor="k",
    width=width,
    # hatch="/",
    label="RelGo",
)
plt.bar(
    xaxis + width,
    y_graindb,
    color="salmon",
    edgecolor="k",
    width=width,
    # hatch="\\",
    label="GrainDB",
)
plt.bar(
    xaxis + width * 2,
    y_back_to_hash,
    color="gold",
    edgecolor="k",
    width=width,
    # hatch="/",
    label="RelGoHash",
)
plt.bar(
    xaxis + width * 3,
    y_duckdb,
    color="lightskyblue",
    edgecolor="k",
    width=width,
    # hatch="\\",
    label="DuckDB",
)


plt.xticks(xaxis_final, xlabel, fontsize=28)
plt.yticks(fontsize=28)
plt.ylim((20, 9000))

plt.margins(0.08)

plt.xlabel("Queries of JOB[*]", fontsize=28)
plt.ylabel("Time Cost (ms)", fontsize=28)

plt.yscale("log")

plt.grid(linestyle="--", axis="y")

plt.legend(
    loc="upper center",
    fontsize=24,
    ncol=4,
    columnspacing=1.5,
    bbox_to_anchor=(0.5, 1.04),
)
plt.tight_layout()
# plt.show()

plt.savefig("../paper/figures/exp/hash_plan_job.pdf", bbox_inches="tight")
