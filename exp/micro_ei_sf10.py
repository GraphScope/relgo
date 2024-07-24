import matplotlib.pyplot as plt
import numpy as np

# matplotlib.rcParams.update({'font.size': 22})

xlabel = [r"QC$_1$", r"QC$_2$", r"QC$_3$"]

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))

y_woei = [4869, 4491, 0]

y_ours = [3893, 3784, 180559]

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
    y_woei,
    color="salmon",
    edgecolor="k",
    width=width,
    # hatch="\\",
    label="RelgoNoEI",
)

plt.xticks(xaxis_final, xlabel, fontsize=32)
plt.yticks(fontsize=32)

plt.margins(0.08)

plt.ylabel("Time Cost (ms)", fontsize=32)

plt.yscale("log")

plt.grid(linestyle="--", axis="y")

plt.legend(loc="upper left", fontsize=32)
plt.tight_layout()
# plt.show()

plt.savefig("../paper/figures/exp/ablation_ei_sf10.pdf", bbox_inches="tight")
