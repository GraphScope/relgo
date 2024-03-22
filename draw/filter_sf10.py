import matplotlib.pyplot as plt
import numpy as np

xlabel = ['2-1', '6-1', '6-2', '9-1', '9-2', '11-1', '11-2']

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))


y_wo_filter_rule = [384.76,14.24,592.85,98.52,1122.06,6.02,32.51]

y_ours = [210.96,13.88,524.21,64.75,789.76,6.0,24.43]

total_width, n = 0.6, 2
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.bar(xaxis, y_ours, color='lightgreen', edgecolor='k', width=width, hatch='/', label='Relgo')
plt.bar(xaxis + width, y_wo_filter_rule, color='salmon', edgecolor='k', width=width, hatch='\\', label='Relgo w.o. Inter')



plt.xticks(xaxis_final, xlabel, rotation=-45, fontsize=22)
plt.yticks(fontsize=22)
plt.ylim((1, 10000))

plt.margins(0.08)

plt.xlabel("SP Queries", fontsize=22)
plt.ylabel("Time Cost (ms)", fontsize=22)

plt.yscale('log')

plt.legend(loc="upper left", fontsize=16)
plt.tight_layout()
# plt.show()

plt.savefig('../paper/figures/exp/filter_sf10.pdf', bbox_inches='tight')