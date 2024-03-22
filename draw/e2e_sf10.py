import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np

xlabel = ['1-1', '1-2', '1-3', '2-1', '3-1', '3-2', '4-1', '5-1', '5-2', '6-1', '6-2', '7-1', '8-1', '9-1', '9-2', '11-1', '11-2', '12-1']

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))
data_per_row = 18

y_duckdb = [17.84,60.27,523.16,744.78,2171.57,29842.29,332.75,625.39,3072.73,310.65,970.33, 618.65,961.29,300.76,1640.37,17.53,72.31,1412.12]

y_graindb = [5.24,28.92,501.73,235.47,2540.67,30155.29,146.78,197.49,2694.96,88.65,592.61,92.18,18.78,73.02,855.31,7.53,37.63,953.45]

y_ours = [3.25,25.96,198.80,210.96,1223.14,28352.51,36.80,62.86,2027.16,13.88,524.21,20.00,17.55,64.75,789.76,6.0,24.43,338.06]

gs = gridspec.GridSpec(1, 1)
gs.update(wspace=0.8)

fig = plt.figure(figsize=(12,5))

plt1 = fig.add_subplot(gs[0])

plt_list = [plt1]
plt_num = len(plt_list)


total_width, n = 0.6, 3
width = total_width / n

xaxis = xaxis - (total_width - width) / 2

for i in range(plt_num):
	start = i * data_per_row
	end = (i + 1) * data_per_row - 1

	plt_cur = plt_list[i]

	plt_cur.bar(xaxis[start:end+1], y_ours[start:end+1], color='lightgreen', edgecolor='k', width=width, hatch='-', label='relgo')
	plt_cur.bar(xaxis[start:end+1] + width, y_graindb[start:end+1], color='salmon', edgecolor='k', width=width, hatch='\\', label='GrainDB')
	plt_cur.bar(xaxis[start:end+1] + width * 2, y_duckdb[start:end+1], color='lightskyblue', edgecolor='k', width=width, hatch='/', label='DuckDB')


	plt_cur.set_xticks(xaxis_final[start:end+1])
	plt_cur.set_xticklabels(xlabel[start:end+1], fontsize=14, rotation=-20)
	plt_cur.set_yticklabels(labels=[], fontsize=18)

	if i == plt_num - 1:
		plt_cur.set_xlabel("Queries", fontsize=14, labelpad=-5)

	plt_cur.set_ylabel("Time Cost (ms)", fontsize=14)

	plt_cur.set_yscale('log')

plt.margins(0.08)

lines, labels = plt_list[0].get_legend_handles_labels()
fig.legend(lines, labels, loc='upper right', fontsize=14)

plt.tight_layout()
# plt.show()

plt.savefig('../paper/figures/exp/e2e_sf10.pdf', bbox_inches='tight')