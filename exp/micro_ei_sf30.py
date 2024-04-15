import matplotlib.pyplot as plt
import numpy as np

# matplotlib.rcParams.update({'font.size': 22})

xlabel = ['QC[1]', 'QC[2]', 'QC[3]']

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))


y_woei = [14487.0, 15712.0, 0]

y_ours = [11088.0, 11906.0, 748070.0]

total_width, n = 0.6, 2
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.bar(xaxis, y_ours, color='lightgreen', edgecolor='k', width=width, hatch='-', label='RelGo')
plt.bar(xaxis + width, y_woei, color='salmon', edgecolor='k', width=width, hatch='\\', label='RelGoNoEI')

plt.xticks(xaxis_final, xlabel, fontsize=22)
plt.yticks(fontsize=22)

plt.margins(0.08)

plt.ylabel("Time Cost (ms)", fontsize=22)

plt.yscale('log')

plt.legend(loc="upper left", fontsize=16)
plt.tight_layout()
# plt.show()

plt.savefig('../paper/figures/exp/ablation_ei_sf30.pdf', bbox_inches='tight')