import matplotlib.pyplot as plt

xlabel = []

for i in range(1, 34):
	xlabel.append(str(i) + 'a')

xaxis = list(range(len(xlabel)))

y_calcite = [600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000]

y_ours = [91, 23, 21, 17, 12, 13, 102, 299, 973, 139, 99, 98, 377, 142, 290, 72, 28, 30, 724, 1269, 285, 1004, 1024, 1820, 93, 2230, 2769, 8256, 28648, 1030, 298, 59, 4987]

plt.plot(xaxis, y_calcite, 's-', color='r', label='Calcite')
plt.plot(xaxis, y_ours, 'o-', color = 'g', label='Relgo')


plt.xticks(xaxis, xlabel, rotation=-45)
plt.yticks(fontsize=22)

plt.margins(0.08)

plt.xlabel("JOB Query", fontsize=22)
plt.ylabel("Opt. Time Cost (ms)", fontsize=22)

plt.yscale('log')

plt.legend(loc="lower right", fontsize=12)
plt.tight_layout()
# plt.show()

plt.savefig('../paper/figures/exp/optimization_job.pdf', bbox_inches='tight')