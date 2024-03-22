import matplotlib.pyplot as plt
import numpy as np

xlabel = ['Triangle', 'Butterfly', '4-Clique']

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))

y_graindb = [68.04, 243158, 74517404.0]

y_ours = [10.90, 43691, 180559]

total_width, n = 0.6, 2
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

plt.bar(xaxis, y_ours, color='lightgreen', edgecolor='k', width=width, hatch='/', label='Relgo')
plt.bar(xaxis + width, y_graindb, color='salmon', edgecolor='k', width=width, hatch='\\', label='GrainDB')


plt.xticks(xaxis_final, xlabel, fontsize=22)
plt.yticks(fontsize=22)

plt.margins(0.08)

plt.xlabel("Queries", fontsize=22)
plt.ylabel("Time Cost (ms)", fontsize=22)

plt.yscale('log')

plt.legend(loc="upper left", fontsize=14)
plt.tight_layout()
# plt.show()

plt.savefig('../paper/figures/exp/hard_sf10.pdf', bbox_inches='tight')