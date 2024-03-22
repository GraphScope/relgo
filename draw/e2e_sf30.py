import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np

xlabel = ['1-1', '1-2', '1-3', '2-1', '3-1', '3-2', '4-1', '5-1', '5-2', '6-1', '6-2', '7-1', '8-1', '9-1', '9-2', '11-1', '11-2', '12-1']

xaxis = np.arange(len(xlabel))
xaxis_final = np.arange(len(xlabel))
data_per_row = 18

y_duckdb = [42.31,175.35,1164.16,2149.51,3882.82,101217.63,861.57,1919.53,8481.12,1040.82,2867.38,2506.98,2900.31,845.06,5070.61,40.25,223.25,2913.57]

y_graindb = [7.86,70.61,1073.82,395.16,3940.12,112099.88,286.14,449.67,6878.43,217.20,1284.63,1172.55,50.39,135.47,2231.10,12.65,94.12,2861.57]

y_ours = [4.14,64.33,376.78,374.47,1781.20,81408.80,64.15,143.51,5419.00,18.67,966.38,39.51,44.57,122.73,2031.98,7.84,47.67,590.45]

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

plt.savefig('../paper/figures/exp/e2e_sf30.pdf', bbox_inches='tight')