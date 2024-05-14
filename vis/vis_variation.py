import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Set Configs
cpu_case_name = "cpugpu/kernelCPU-"
gpu_case_name = "cpugpu/kernelGPU-"

nmax = 3
font_size = 28
logdata = []
BlueList = ['#92B4F4', '#7772CA', '#5E7CE2', '#C6CDFF', '#248BD6', '#0F6BAE']
RedList  = ['#FFB3A7', '#8C4356', '#9D2933']
vis_callback_only = False

# Load File
cpu_data=[]
gpu_lnch=[]
gpu_sync=[]

for i in range(nmax):
    
    cpu_data.append([])
    gpu_lnch.append([])
    gpu_sync.append([])

    fileHandler = open("./logs/" + cpu_case_name + str(i+1) + ".log", "r")
    listOfLines = fileHandler.readlines()
    fileHandler.close()
    
    # Process Raw Data
    for line in listOfLines:
        number = re.findall("\d+", line)
        if len(re.findall("kernelCPU", line)) > 0:
            time = float(number[-2]) + float(number[-1]) / 1000000
            cpu_data[-1].append(time)
    
    fileHandler = open("./logs/" + gpu_case_name + str(i+1) + ".log", "r")
    listOfLines = fileHandler.readlines()
    fileHandler.close()
    
    # Process Raw Data
    for line in listOfLines:
        number = re.findall("\d+", line)
        if len(re.findall("kernelGPU", line)) > 0:
            time = float(number[-2]) + float(number[-1]) / 1000000
            gpu_lnch[-1].append(time)
        if len(re.findall("cudaStreamSynchronize", line)) > 0:
            time = float(number[-2]) + float(number[-1]) / 1000000
            gpu_sync[-1].append(time)

cpu_data[0] = cpu_data[0][:60]
cpu_data[1] = cpu_data[1][:60]
cpu_data[2] = cpu_data[2][:60]

gpu_lnch[0] = gpu_lnch[0][:60]
gpu_lnch[1] = gpu_lnch[1][:60]
gpu_lnch[2] = gpu_lnch[2][:60]

gpu_sync[0] = gpu_sync[0][:60]
gpu_sync[1] = gpu_sync[1][:60]
gpu_sync[2] = gpu_sync[2][:60]

fig, axes = plt.subplots(1, 1, figsize=(16,12))
iters=list(range(60))

def draw_line(name_of_alg,color_index,datas):
    color=BlueList[color_index] if color_index == 0 else RedList[color_index]
    avg=np.mean(datas,axis=0)
    std=np.std(datas,axis=0)
    r1 = list(map(lambda x: x[0]-x[1], zip(avg, std)))#上方差
    r2 = list(map(lambda x: x[0]+x[1], zip(avg, std)))#下方差
    plt.plot(iters, avg, color=color,label=name_of_alg,linewidth=3.5)
    plt.fill_between(iters, r1, r2, color=color, alpha=0.2)

plt.grid()

draw_line("CPU", 0, cpu_data)
draw_line("GPU-Sync", 1, gpu_sync)
import matplotlib.ticker as mtick
axes.yaxis.set_major_formatter(mtick.FormatStrFormatter('%.5f'))
plt.xticks(fontsize=font_size)
plt.yticks(fontsize=font_size)
# Save File
plt.savefig("./figures/cpugpu/cpu-gpu.png", bbox_inches='tight')