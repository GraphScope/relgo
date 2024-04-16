import matplotlib
import matplotlib.pyplot as plt
import numpy as np

# matplotlib.rcParams.update({'font.size': 22})

xlabel = [1, 2, 3, 4, 5, 6, 7, 8]

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))

y_aware = [0, 0, 0, 0, 4, 49, 954, 18699]

y_agnos = [0, 0, 0, 31, 1359, 63991, 0, 0]


total_width, n = 0.6, 2
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.bar(xaxis, y_aware, color='lightgreen', edgecolor='k', width=width, hatch='-', label='Graph-Aware Method')
plt.bar(xaxis + width, y_agnos, color='salmon', edgecolor='k', width=width, hatch='\\', label='Graph-Agnostic Method')


plt.xticks(xaxis_final, xlabel, fontsize=22)
plt.yticks(fontsize=22)

plt.margins(0.08)

plt.xlabel("Path Length", fontsize=22)
plt.ylabel("Time Cost (ms)", fontsize=22)

plt.yscale('log')

plt.legend(loc="upper left", fontsize=16)
plt.tight_layout()
# plt.show()

plt.savefig('../paper/figures/exp/compare_search_time.pdf', bbox_inches='tight')