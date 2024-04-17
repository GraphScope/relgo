import matplotlib.pyplot as plt
import numpy as np

xlabel = [
    "1-1",
    "1-2",
    "1-3",
    "2",
    "3-1",
    "3-2",
    "4",
    "5-1",
    "5-2",
    "6-1",
    "6-2",
    "7",
    "8",
    "9-1",
    "9-2",
    "11-1",
    "11-2",
    "12",
]

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))
data_per_row = 18

y_duckdb = [
    17.84,
    60.27,
    523.16,
    744.78,
    2171.57,
    29842.29,
    332.75,
    625.39,
    3072.73,
    310.65,
    970.33,
    618.65,
    961.29,
    300.76,
    1640.37,
    17.53,
    72.31,
    1412.12,
]

y_graindb = [
    5.24,
    28.92,
    501.73,
    235.47,
    2540.67,
    30155.29,
    146.78,
    197.49,
    2694.96,
    88.65,
    592.61,
    92.18,
    18.78,
    73.02,
    855.31,
    7.53,
    37.63,
    953.45,
]

y_ours = [
    3.25,
    25.96,
    198.80,
    210.96,
    1223.14,
    28352.51,
    36.80,
    62.86,
    2027.16,
    13.88,
    524.21,
    20.00,
    17.55,
    64.75,
    789.76,
    6.0,
    24.43,
    338.06,
]

total_width, n = 0.7, 3
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.figure(figsize=(15, 5))

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
    y_duckdb,
    color="lightskyblue",
    edgecolor="k",
    width=width,
    # hatch="\\",
    label="DuckDB",
)
plt.xticks(xaxis_final, xlabel, fontsize=20)
plt.yticks(fontsize=22)
plt.yscale("log")

plt.grid(linestyle="--", axis="y")

plt.xlabel("Queries of IC[*]", fontsize=24)
plt.ylabel("Time Cost (ms)", fontsize=22)

plt.legend(
    loc="upper center", fontsize=26, ncol=3, bbox_to_anchor=(0.5, 1.32), columnspacing=2
)

plt.tight_layout()
# plt.show()
plt.savefig("../paper/figures/exp/e2e_sf10.pdf", bbox_inches="tight")
