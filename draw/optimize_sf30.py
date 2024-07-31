import matplotlib.pyplot as plt

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
xaxis = list(range(1, 2 * len(xlabel), 2))

y_calcite = [
    902,
    2030,
    10351,
    389,
    2600,
    17023,
    25926,
    600000,
    600000,
    12700,
    55401,
    600000,
    423,
    408,
    2263,
    5707,
    25796,
    455701,
]

y_ours = [13, 16, 20, 7, 46, 51, 21, 17, 19, 16, 19, 13, 15, 1, 7, 42, 23, 48]

plt.figure(figsize=(15, 5))

plt.plot(xaxis, y_calcite, "s-", color="r", label="Calcite")
plt.plot(xaxis, y_ours, "o-", color="g", label="RelGo")


plt.xticks(xaxis, xlabel, fontsize=32, rotation=-45)
plt.yticks(fontsize=26)

plt.margins(0.08)

plt.xlabel(r"Queries of IC$_*$", fontsize=28)
plt.ylabel("Opt. Time (ms)", fontsize=26)

plt.yscale("log")

plt.legend(loc="lower right", fontsize=18, ncol=2, columnspacing=1)
plt.tight_layout()
# plt.show()

plt.ylim(bottom=0.01)

plt.savefig("../paper/figures/exp/optimization_sf30.pdf", bbox_inches="tight")
