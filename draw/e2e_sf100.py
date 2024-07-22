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

with open('e2e_sf100.txt', 'r') as f:
    lines = f.readlines()
    y_duckdb = [float(x) for x in lines[0].split(',')]
    y_graindb = [float(x) for x in lines[1].split(',')]
    y_umbra = [float(x) for x in lines[2].split(',')]
    y_ours = [float(x) for x in lines[3].split(',')]

total_width, n = 0.7, 4
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
    y_umbra,
    color="black",
    edgecolor="k",
    width=width,
    # hatch="/",
    label="Umbra",
)

plt.bar(
    xaxis + width * 2,
    y_graindb,
    color="salmon",
    edgecolor="k",
    width=width,
    # hatch="\\",
    label="GrainDB",
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
plt.xticks(xaxis_final, xlabel, fontsize=20)
plt.yticks(fontsize=22)
plt.yscale("log")

plt.grid(linestyle="--", axis="y")

plt.xlabel("Queries of IC[*]", fontsize=24)
plt.ylabel("Time Cost (ms)", fontsize=22)

plt.tight_layout()
# plt.show()
plt.savefig("../paper/figures/exp/e2e_sf100.pdf", bbox_inches="tight")
