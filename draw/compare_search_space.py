import matplotlib
import matplotlib.pyplot as plt
import numpy as np

# matplotlib.rcParams.update({'font.size': 22})

y_aware = [
    2,
    16,
    192,
    2816,
    46080,
    806912,
    14794752,
    280428544,
    5450760192,
    108054708224,
]

y_agnos = [
    8,
    224,
    8448,
    366080,
    17199104,
    852017152,
    43818024960,
    2317200261120,
    125210119372800,
    6882979133521920,
]

xlabel = list(range(1, len(y_aware) + 1))

plt.figure(figsize=(8, 6))
plt.plot(
    xlabel,
    y_aware,
    color="#5CCCCF",
    linewidth=3,
    marker="s",
    markersize=10,
    label="Graph-Aware",
)
plt.plot(
    xlabel,
    y_agnos,
    color="#C53932",
    linewidth=3,
    marker="o",
    markersize=10,
    label="Graph-Agnostic",
)


plt.xticks(xlabel, xlabel, fontsize=28)
plt.yticks(fontsize=28)

plt.xlabel("Path Length", fontsize=28)
plt.ylabel("Search Space", fontsize=28)

plt.yscale("log")

plt.grid(linestyle="--", axis="y")

plt.legend(loc="upper left", fontsize=26)
plt.tight_layout()
# plt.show()

plt.savefig("../paper/figures/exp/compare_search_space.pdf", bbox_inches="tight")

plt.figure(figsize=(8, 6))
# plt.ticklabel_format(axis="y", style="sci", scilimits=(0, 0))
# plt.gca().ticklabel_format(useMathText=True)

plt.plot(
    xlabel,
    [y_agnos[i] / y_aware[i] for i in range(len(y_aware))],
    # color="blue",
    linewidth=3,
    marker="^",
    markersize=10,
    label="Search Space Ratio",
)

plt.xticks(xlabel, xlabel, fontsize=28)
plt.yticks(fontsize=28)
plt.yscale("log")
# plt.yticks([10000, 20000, 30000, 40000, 50000, 60000], [, 2, 3, 4, 5, 6], fontsize=28)

plt.xlabel("Path Length", fontsize=28)
plt.ylabel("Search Space Ratio", fontsize=28)

plt.grid(linestyle="--", axis="y")
plt.legend(loc="upper left", fontsize=26)
plt.tight_layout()
# plt.show()

plt.savefig("../paper/figures/exp/compare_search_space_ratio.pdf", bbox_inches="tight")
