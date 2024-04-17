import matplotlib.pyplot as plt
import numpy as np

y_duckdb_complete = [
    379,
    424,
    1383,
    782,
    387,
    932,
    1120,
    1349,
    2541,
    1745,
    313,
    843,
    1386,
    1339,
    1026,
    1083,
    3185,
    2966,
    3279,
    2267,
    1211,
    1576,
    1818,
    3783,
    2750,
    3416,
    931,
    2231,
    2785,
    2920,
    2789,
    46,
    520,
]

y_graindb_complete = [
    224,
    221,
    594,
    517,
    366,
    330,
    451,
    613,
    1082,
    714,
    214,
    506,
    393,
    285,
    528,
    382,
    2797,
    2233,
    1287,
    1231,
    221,
    543,
    492,
    1580,
    2251,
    1028,
    295,
    1340,
    487,
    1449,
    2165,
    10,
    348,
]

y_ours_complete = [
    35,
    143,
    320,
    348,
    49,
    53,
    107,
    455,
    770,
    620,
    47,
    211,
    257,
    240,
    399,
    286,
    2249,
    2087,
    1202,
    1186,
    62,
    482,
    327,
    1194,
    2244,
    682,
    136,
    1045,
    314,
    1235,
    2043,
    8,
    181,
]

xlabel = []

for i in range(1, 18):
    xlabel.append(str(i) + "a")


xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))
data_per_row = 18


total_width, n = 0.8, 3
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.figure(figsize=(15, 4))
y_ours = y_ours_complete[:17]
y_graindb = y_graindb_complete[:17]
y_duckdb = y_duckdb_complete[:17]

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
plt.xticks(xaxis_final, xlabel, fontsize=22, rotation=-20)
plt.yticks(fontsize=22)
plt.yscale("log")
plt.grid(linestyle="--", axis="y")
plt.ylabel("Time Cost (ms)", fontsize=22)

plt.tight_layout()
# plt.show()
plt.savefig("../paper/figures/exp/e2e_job_part1.pdf", bbox_inches="tight")

xlabel = []

for i in range(18, 34):
    xlabel.append(str(i) + "a")


xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))
data_per_row = 18


total_width, n = 0.8, 3
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.figure(figsize=(15, 4))
y_ours = y_ours_complete[17:]
y_graindb = y_graindb_complete[17:]
y_duckdb = y_duckdb_complete[17:]

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
plt.xticks(xaxis_final, xlabel, fontsize=22, rotation=-20)
plt.yticks(fontsize=22)
plt.yscale("log")
plt.grid(linestyle="--", axis="y")
plt.ylabel("Time Cost (ms)", fontsize=22)

plt.tight_layout()
# plt.show()
plt.savefig("../paper/figures/exp/e2e_job_part2.pdf", bbox_inches="tight")
