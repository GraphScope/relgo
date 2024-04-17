import matplotlib.pyplot as plt
import numpy as np

xlabel = ["2-1", "6-1", "6-2", "9-1", "9-2", "11-1", "11-2"]

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))


y_wo_filter_rule = [812.67, 18.53, 1001.25, 223.45, 2942.51, 8.24, 61.02]

y_ours = [374.47, 18.67, 966.38, 122.73, 2031.98, 7.84, 47.67]

total_width, n = 0.8, 2
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.figure(figsize=(10, 8))

plt.bar(
    xaxis,
    y_ours,
    color="lightgreen",
    edgecolor="k",
    width=width,
    hatch="/",
    label="Relgo",
)
plt.bar(
    xaxis + width,
    y_wo_filter_rule,
    color="salmon",
    edgecolor="k",
    width=width,
    hatch="\\",
    label="Relgo w.o. Inter",
)


plt.xticks(xaxis_final, xlabel, rotation=-45, fontsize=28)
plt.yticks(fontsize=28)
plt.ylim((1, 30000))

plt.margins(0.08)

plt.xlabel("SP Queries", fontsize=28)
plt.ylabel("Time Cost (ms)", fontsize=28)

plt.yscale("log")

plt.legend(
    loc="upper center",
    bbox_to_anchor=(0.5, 1),
    ncol=3,
    frameon=True,
    handletextpad=0.1,
    columnspacing=2,
    prop={"size": 22},
)
plt.tight_layout()
# plt.show()

plt.savefig("../paper/figures/exp/filter_sf30.pdf", bbox_inches="tight")
