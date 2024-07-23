import matplotlib.pyplot as plt
import numpy as np

with open('e2e_job.txt', 'r') as f:
    lines = f.readlines()
    y_duckdb_complete = [float(x) for x in lines[0].split(',')]
    y_graindb_complete = [float(x) for x in lines[1].split(',')]
    y_umbra_complete = [float(x) for x in lines[2].split(',')]
    y_kuzu_complete = [float(x) for x in lines[3].split(',')]
    y_ours_complete = [float(x) for x in lines[4].split(',')]


xlabel = []

for i in range(1, 18):
    xlabel.append(str(i))


xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))
data_per_row = 18


total_width, n = 0.7, 5
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.figure(figsize=(15, 5.5))
y_ours = y_ours_complete[:17]
y_kuzu = y_kuzu_complete[:17]
y_umbra = y_umbra_complete[:17]
y_graindb = y_graindb_complete[:17]
y_duckdb = y_duckdb_complete[:17]

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
    # hatch='\\',
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
    [b/a for a, b in zip(y_kuzu, y_duckdb)],
    color="gold",
    edgecolor="k",
    width=width,
    # hatch="/",
    label="Kùzu",
)

plt.xticks(xaxis_final, xlabel, fontsize=28, rotation=0)
plt.yticks(fontsize=28)
plt.yscale("log")
plt.grid(linestyle="--", axis="y")
plt.xlabel("Queries of JOB[*]", fontsize=28)
plt.ylabel("Speedup vs. DuckDB", fontsize=26)

plt.ylim(top=100)

plt.tight_layout()
# plt.show()
plt.savefig("../paper/figures/exp/e2e_job_part1.pdf", bbox_inches="tight")

xlabel = []

for i in range(18, 34):
    xlabel.append(str(i))


xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))
data_per_row = 18


total_width, n = 0.7, 5
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.figure(figsize=(15, 5.5))
y_ours = y_ours_complete[17:]
y_kuzu = y_kuzu_complete[17:]
y_umbra = y_umbra_complete[17:]
y_graindb = y_graindb_complete[17:]
y_duckdb = y_duckdb_complete[17:]

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
    # hatch='\\',
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
    [b/a for a, b in zip(y_kuzu, y_duckdb)],
    color="gold",
    edgecolor="k",
    width=width,
    # hatch="/",
    label="Kùzu",
)

plt.xticks(xaxis_final, xlabel, fontsize=28, rotation=0)
plt.yticks(fontsize=28)
plt.yscale("log")
plt.grid(linestyle="--", axis="y")
plt.xlabel("Queries of JOB[*]", fontsize=28)
plt.ylabel("Speedup vs. DuckDB", fontsize=26)

plt.ylim(top=100)

plt.tight_layout()
# plt.show()
plt.savefig("../paper/figures/exp/e2e_job_part2.pdf", bbox_inches="tight")
