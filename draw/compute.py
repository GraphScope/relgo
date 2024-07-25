import numpy as np

with open('e2e_job.txt', 'r') as f:
    lines = f.readlines()
    y_duckdb_job = [float(x) for x in lines[0].split(',')]
    y_graindb_job = [float(x) for x in lines[1].split(',')]
    y_umbra_job = [float(x) for x in lines[2].split(',')]
    y_kuzu_job = [float(x) for x in lines[3].split(',')]
    y_ours_job = [float(x) for x in lines[4].split(',')]

with open('e2e_sf10.txt', 'r') as f:
    lines = f.readlines()
    y_duckdb_sf10 = [float(x) for x in lines[0].split(',')]
    y_graindb_sf10 = [float(x) for x in lines[1].split(',')]
    y_umbra_sf10 = [float(x) for x in lines[2].split(',')]
    y_kuzu_sf10 = [float(x) for x in lines[3].split(',')]
    y_ours_sf10 = [float(x) for x in lines[4].split(',')]

with open('e2e_sf30.txt', 'r') as f:
    lines = f.readlines()
    y_duckdb_sf30 = [float(x) for x in lines[0].split(',')]
    y_graindb_sf30 = [float(x) for x in lines[1].split(',')]
    y_umbra_sf30 = [float(x) for x in lines[2].split(',')]
    y_kuzu_sf30 = [float(x) for x in lines[3].split(',')]
    y_ours_sf30 = [float(x) for x in lines[4].split(',')]


with open('e2e_sf100.txt', 'r') as f:
    lines = f.readlines()
    y_duckdb_sf100 = [float(x) for x in lines[0].split(',')]
    y_graindb_sf100 = [float(x) for x in lines[1].split(',')]
    y_umbra_sf100 = [float(x) for x in lines[2].split(',')]
    y_kuzu_sf100 = [float(x) for x in lines[3].split(',')]
    y_ours_sf100 = [float(x) for x in lines[4].split(',')]


start = 0
end = len(y_ours_sf10)
duckdb_ratio_sum = 0
graindb_ratio_sum = 0
umbra_ratio_sum = 0
kuzu_ratio_sum = 0

max_ratio = -1
min_ratio = -1

zero_num = 0

for i in range(start, end):
    duckdb_ratio_sum += (y_duckdb_job[i]) / (y_ours_job[i])
    graindb_ratio_sum += (y_graindb_job[i]) / (y_ours_job[i])
    umbra_ratio_sum += (y_umbra_job[i]) / (y_ours_job[i])
    kuzu_ratio_sum += (y_kuzu_job[i] / y_ours_job[i])

    if y_kuzu_job[i] / y_ours_job[i] == 0:
        zero_num += 1
    

# print(max_ratio)
# print(min_ratio)
print("JOB: ")
print("Kuzu: ", kuzu_ratio_sum / (end - start - zero_num))
print("DuckDB: ", duckdb_ratio_sum / (end - start))
print("GrainDB: ", graindb_ratio_sum / (end - start))
print("Umbra: ", umbra_ratio_sum / (end - start))


