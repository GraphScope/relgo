import matplotlib.pyplot as plt
import numpy as np

xlabel = ['2-1', '6-1', '6-2', '9-1', '9-2', '11-1', '11-2']

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))


y_wo_filter_rule = [812.67,18.53,1001.25,223.45,2942.51,8.24,61.02]

y_ours = [374.47,18.67,966.38,122.73,2031.98,7.84,47.67]

total_width, n = 0.6, 2
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.bar(xaxis, y_ours, color='lightgreen', edgecolor='k', width=width, hatch='/', label='Relgo')
plt.bar(xaxis + width, y_wo_filter_rule, color='salmon', edgecolor='k', width=width, hatch='\\', label='Relgo w.o. Inter')



plt.xticks(xaxis_final, xlabel, rotation=-45, fontsize=22)
plt.yticks(fontsize=22)
plt.ylim((1, 30000))

plt.margins(0.08)

plt.xlabel("SP Queries", fontsize=22)
plt.ylabel("Time Cost (ms)", fontsize=22)

plt.yscale('log')

plt.legend(loc="upper left", fontsize=16)
plt.tight_layout()
# plt.show()

plt.savefig('../paper/figures/exp/filter_sf30.pdf', bbox_inches='tight')