import matplotlib.pyplot as plt

xlabel = []

for i in range(1, 34):
    if i <= 3 or i >= 33:
        xlabel.append(str(i))
    elif i >= 12 and i <= 14:
        xlabel.append('.')
    else:
        xlabel.append('')
        

xaxis = list(range(len(xlabel)))

y_calcite = [600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000, 600000]

y_ours = [9, 1, 5, 5, 9, 8, 27, 28, 51, 76, 44, 34, 42, 24, 55, 37, 26, 14, 72, 84, 21, 45, 60, 74, 24, 106, 78, 80, 210, 58, 39, 12, 113]


plt.figure(figsize=(15, 4))

plt.plot(xaxis, y_calcite, 's-', color='r', label='Calcite')
plt.plot(xaxis, y_ours, 'o-', color = 'g', label='Relgo')


plt.xticks(xaxis, xlabel, fontsize=32, rotation=0)
plt.yticks(fontsize=26)

plt.margins(0.08)

plt.xlabel("Queries of JOB[*]", fontsize=28)
plt.ylabel("Opt. Time (ms)", fontsize=26)

plt.yscale('log')

plt.legend(loc="lower right", fontsize=18)
plt.tight_layout()
# plt.show()

plt.savefig('../paper/figures/exp/optimization_job.pdf', bbox_inches='tight')