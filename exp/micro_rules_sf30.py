import matplotlib.pyplot as plt
import numpy as np

xlabel = ["QR[1]", "QR[2]", "QR[3]", "QR[4]"]

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))


y_wo_filter_rule = [2670.55, 2880.29, 6.98, 447.24]

y_ours = [1.94, 125.12, 2.35, 269.65]

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


plt.xticks(xaxis_final, xlabel, fontsize=28)
plt.yticks(fontsize=28)
plt.ylim((1, 30000))

plt.margins(0.08)

plt.ylabel("Time Cost (ms)", fontsize=28)

plt.yscale("log")

plt.grid(linestyle="--", axis="y")

plt.legend(loc="upper center", fontsize=32, bbox_to_anchor=(0.5, 1), ncol=2)
plt.tight_layout()
# plt.show()

plt.savefig("../paper/figures/exp/filter_sf30.pdf", bbox_inches="tight")
