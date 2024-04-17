import matplotlib.pyplot as plt
import numpy as np

xlabel = []

for i in range(1, 11):
    xlabel.append(str(i) + "a")

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))

y_duckdb = [379, 424, 1383, 782, 387, 932, 1120, 1349, 2541, 1745]

y_back_to_hash = [329, 288, 1040, 480, 128, 946, 737, 1376, 2337, 1728]

y_graindb = [224, 221, 594, 517, 366, 330, 451, 613, 1082, 714]

y_ours = [35, 143, 320, 348, 49, 53, 107, 455, 770, 620]


total_width, n = 0.8, 4
width = total_width / n

xaxis = xaxis - (total_width - width) / 2


plt.bar(
    xaxis,
    y_ours,
    color="lightgreen",
    edgecolor="k",
    width=width,
    hatch="-",
    label="Relgo",
)
plt.bar(
    xaxis + width,
    y_graindb,
    color="salmon",
    edgecolor="k",
    width=width,
    hatch="\\",
    label="GrainDB",
)
plt.bar(
    xaxis + width * 2,
    y_back_to_hash,
    color="gold",
    edgecolor="k",
    width=width,
    hatch="/",
    label="Relgo-Hash",
)
plt.bar(
    xaxis + width * 3,
    y_duckdb,
    color="lightskyblue",
    edgecolor="k",
    width=width,
    hatch="*",
    label="DuckDB",
)


plt.xticks(xaxis_final, xlabel, fontsize=18)
plt.yticks(fontsize=22)
plt.ylim((20, 9000))

plt.margins(0.08)

# plt.xlabel("JOB Query", fontsize=22)
plt.ylabel("Time Cost (ms)", fontsize=22)

plt.yscale("log")

plt.legend(loc="upper left", fontsize=12)
plt.tight_layout()
# plt.show()

plt.savefig("../paper/figures/exp/hash_plan_job.pdf", bbox_inches="tight")
