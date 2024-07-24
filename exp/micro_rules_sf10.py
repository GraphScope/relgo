import matplotlib.pyplot as plt
import numpy as np

xlabel = [r"QR$_1$", r"QR$_2$", r"QR$_3$", r"QR$_4$"]

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))


y_wo_filter_rule = [757.10, 910.80, 3.25, 194.43]

y_ours = [1.31, 43.67, 1.47, 108.41]

total_width, n = 0.6, 2
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.figure(figsize=(10, 6))

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
    y_wo_filter_rule,
    color="salmon",
    edgecolor="k",
    width=width,
    # hatch="\\",
    label="RelGoNoRule",
)


plt.xticks(xaxis_final, xlabel, fontsize=32)
plt.yticks(fontsize=32)
plt.ylim((1, 10000))

plt.margins(0.08)

plt.ylabel("Time Cost (ms)", fontsize=32)

plt.yscale("log")

plt.grid(linestyle="--", axis="y")

plt.legend(loc="upper center", fontsize=32, bbox_to_anchor=(0.5, 1), ncol=2)
plt.tight_layout()
# plt.show()

plt.savefig("../paper/figures/exp/filter_sf10.pdf", bbox_inches="tight")
