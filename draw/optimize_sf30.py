import matplotlib.pyplot as plt

xlabel = ['1-1', '1-2', '1-3', '2-1', '3-1', '3-2', '4-1', '5-1', '5-2', '6-1', '6-2', '7-1', '8-1', '9-1', '9-2', '11-1', '11-2', '12-1']
xaxis = list(range(1, 2 * len(xlabel), 2))

y_calcite = [902, 2030, 10351, 389, 2600, 17023, 25926, 600000, 600000, 12700, 55401, 600000, 423, 408, 2263, 5707, 25796, 455701]

y_ours = [89, 48, 96, 16, 194, 405, 79, 69, 93, 70, 157, 26, 35, 3, 18, 24, 56, 407]

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

plt.savefig('../paper/figures/exp/optimization_sf30.pdf', bbox_inches='tight')