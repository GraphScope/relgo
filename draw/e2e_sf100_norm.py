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
    y_kuzu = [float(x) for x in lines[3].split(',')]
    y_ours = [float(x) for x in lines[4].split(',')]

total_width, n = 0.7, 5
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.figure(figsize=(15, 5))

plt.bar(
    xaxis,
    [b/a for a, b in zip(y_ours, y_duckdb)],
    color="lightgreen",
    edgecolor="k",
    width=width,
    # hatch="/",
    label="RelGo",
)

plt.bar(
    xaxis + width,
    [b/a for a, b in zip(y_umbra, y_duckdb)],
    color="black",
    edgecolor="k",
    width=width,
    # hatch="/",
    label="Umbra",
)

plt.bar(
    xaxis + width * 2,
    [b/a for a, b in zip(y_graindb, y_duckdb)],
    color="salmon",
    edgecolor="k",
    width=width,
    # hatch="\\",
    label="GrainDB",
)

plt.bar(
    xaxis + width * 3,
    [b/a for a, b in zip(y_duckdb, y_duckdb)],
    color="lightskyblue",
    edgecolor="k",
    width=width,
    # hatch="\\",
    label="DuckDB",
)

plt.bar(
    xaxis + width * 4,
    [b/a if a != 0 else 0 for a, b in zip(y_kuzu, y_duckdb)],
    color="gold",
    edgecolor="k",
    width=width,
    # hatch="/",
    label="Kùzu",
)

plt.xticks(xaxis_final, xlabel, fontsize=28, rotation=-60)
plt.yticks(fontsize=28)
plt.yscale("log")

plt.grid(linestyle="--", axis="y")

plt.xlabel("Queries of IC[*]", fontsize=28)
plt.ylabel("Speedup vs. DuckDB", fontsize=26)

plt.tight_layout()
# plt.show()
plt.savefig("../paper/figures/exp/e2e_sf100.pdf", bbox_inches="tight")
