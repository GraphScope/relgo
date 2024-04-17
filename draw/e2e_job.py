import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np

xlabel = []

for i in range(1, 34):
    xlabel.append(str(i) + "a")


xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))
data_per_row = 18

y_duckdb = [
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

y_graindb = [
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

y_ours = [
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

gs = gridspec.GridSpec(2, 1)
gs.update(wspace=0.8)

fig = plt.figure(figsize=(12, 5))

plt1 = fig.add_subplot(gs[0])
plt2 = fig.add_subplot(gs[1])

plt_list = [plt1, plt2]
plt_num = len(plt_list)


total_width, n = 0.6, 3
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

for i in range(plt_num):
    start = i * data_per_row
    end = (i + 1) * data_per_row - 1

    plt_cur = plt_list[i]

    plt_cur.bar(
        xaxis[start : end + 1],
        y_ours[start : end + 1],
        color="lightgreen",
        edgecolor="k",
        width=width,
        hatch="-",
        label="relgo",
    )
    plt_cur.bar(
        xaxis[start : end + 1] + width,
        y_graindb[start : end + 1],
        color="salmon",
        edgecolor="k",
        width=width,
        hatch="\\",
        label="GrainDB",
    )
    plt_cur.bar(
        xaxis[start : end + 1] + width * 2,
        y_duckdb[start : end + 1],
        color="lightskyblue",
        edgecolor="k",
        width=width,
        hatch="/",
        label="DuckDB",
    )

    plt_cur.set_xticks(xaxis_final[start : end + 1])
    plt_cur.set_xticklabels(xlabel[start : end + 1], fontsize=10, rotation=-20)
    # plt_cur.set_yticklabels(fontsize=22)

    if i == plt_num - 1:
        plt_cur.set_xlabel("Queries", fontsize=14, labelpad=-3)

    plt_cur.set_ylabel("Time Cost (ms)", fontsize=14)

    plt_cur.set_yscale("log")

plt.margins(0.08)

lines, labels = plt_list[0].get_legend_handles_labels()
fig.legend(lines, labels, loc="upper right")

plt.tight_layout()
# plt.show()

plt.savefig("../paper/figures/exp/e2e_job.pdf", bbox_inches="tight")
