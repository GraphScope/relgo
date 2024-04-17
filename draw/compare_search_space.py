import matplotlib
import matplotlib.pyplot as plt
import numpy as np

# matplotlib.rcParams.update({'font.size': 22})

xlabel = [1, 2, 3, 4, 5, 6, 7, 8]

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))

y_aware = [2, 16, 192, 2816, 46080, 806912, 14794752, 280428544]

y_agnos = [8, 224, 8448, 366080, 17199104, 852017152, 0, 0]


total_width, n = 0.6, 2
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.figure(figsize=(15, 5))

plt.bar(
    xaxis,
    y_aware,
    color="lightgreen",
    edgecolor="k",
    width=width,
    label="Graph-Aware Method",
)
plt.bar(
    xaxis + width,
    y_agnos,
    color="salmon",
    edgecolor="k",
    width=width,
    label="Graph-Agnostic Method",
)


plt.xticks(xaxis_final, xlabel, fontsize=28)
plt.yticks(fontsize=28)

plt.margins(0.08)

plt.xlabel("Path Length", fontsize=28)
plt.ylabel("Search Space", fontsize=28)

plt.yscale("log")

plt.grid(linestyle="--", axis="y")

plt.legend(loc="upper left", fontsize=28)
plt.tight_layout()
# plt.show()

plt.savefig("../paper/figures/exp/compare_search_space.pdf", bbox_inches="tight")
