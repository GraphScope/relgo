import matplotlib.pyplot as plt

xlabel = ['1-1', '1-2', '1-3', '2-1', '3-1', '3-2', '4-1', '5-1', '5-2', '6-1', '6-2', '7-1', '8-1', '9-1', '9-2', '11-1', '11-2', '12-1']
xaxis = list(range(1, 2 * len(xlabel), 2))

y_calcite = [856, 2138, 10201, 394, 2719, 17026, 25839, 600000, 600000, 12431, 53586, 600000, 426, 411, 2044, 5466, 22544, 473923]

y_ours = [92, 73, 91, 15, 184, 407, 60, 52, 102, 107, 157, 26, 33, 3, 18, 23, 56, 399]

plt.plot(xaxis, y_calcite, 's-', color='r', label='Calcite')
plt.plot(xaxis, y_ours, 'o-', color = 'g', label='Relgo')


plt.xticks(xaxis, xlabel, rotation=-45, fontsize=14)
plt.yticks(fontsize=22)

plt.margins(0.08)

plt.xlabel("IC Query", fontsize=22)
plt.ylabel("Opt. Time Cost (ms)", fontsize=22)

plt.yscale('log')

plt.legend(loc="lower right", fontsize=12)
plt.tight_layout()
# plt.show()

plt.savefig('../paper/figures/exp/optimization_sf10.pdf', bbox_inches='tight')