from datetime import date
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import glob
import os
import csv
import gc

owd = os.getcwd()
all_company = os.listdir(os.chdir('result'))
oowd = os.getcwd()

file_extension = '.csv'
splitTargetFolder = ['/testBestHold']

# for targetFolder in splitTargetFolder:
#     for comName in all_company:
#         if comName != '.DS_Store':
#             os.chdir(comName+targetFolder)
#             all_filename = [i for i in glob.glob(f"*{file_extension}")]
#             for file in all_filename:
#                 if (targetFolder == '/testBestHold' and len(file.split('_')) == 2) or (targetFolder == '/specify' and file.split('_')[0] == 'hold' and len(file.split('_')) == 6):
#                     print('spliting ' + file)
#                     year = [str(i) for i in range(2012, 2022)]
#                     yIndex = []
#                     yIN = -1
#                     yIndex.append(int(1))
#                     with open(file, 'rt') as f:
#                         reader = csv.reader(f, delimiter=',')
#                         for y in year:
#                             for row in reader:
#                                 yIN += 1
#                                 if y == row[0].split('-')[0]:
#                                     yIndex.append(int(yIN))
#                                     yIndex.append(int(yIN))
#                                     break
#                         f.close()
#                     csvfile = open(''.join(file), 'r').readlines()
#                     yIndex.append(len(csvfile))
#                     for i in range(len(yIndex)):
#                         if i % 2 == 0:
#                             if targetFolder == '/testBestHold':
#                                 f = open(
#                                     file.split('.')[0] + '_' + str(csvfile[yIndex[i]]).split('-')[0] + '_hold.csv', 'w+')
#                             elif targetFolder == '/specify':
#                                 f = open(
#                                     file.split('.')[0] + '_' + str(csvfile[yIndex[i]]).split('-')[0] + '.csv', 'w+')
#                             f.write('Date,Price,Hold\n')
#                             f.writelines(csvfile[yIndex[i]:yIndex[i+1]])
#                             f.close()
#         os.chdir(oowd)
# now = 0
fig = plt.figure(figsize=[16, 4.5], dpi=300)
# for targetFolder in splitTargetFolder:
#     for comName in all_company:
#         if now < 1:
#             print(comName)
#             os.chdir(comName+targetFolder)
#             NewAll_filename = [i for i in glob.glob(f"*{file_extension}")]
#             print(NewAll_filename)
#             for files in NewAll_filename:
#                 if files.split('_')[0] != 'RoR':
#                     df = pd.read_csv(files)
#                     month = ['01', '02', '03', '04',
#                              '05', '06', '07', '08', '09', '10', '11', '12']
#                     year = ['2011', '2012', '2013', '2014', '2015',
#                             '2016', '2017', '2018', '2019', '2020']
#                     xIndex = []
#                     dfIndex = 0
#                     if len(files.split('_')) == 4 or len(files.split('_')) == 7:
#                         for m in month:
#                             for row in df.Date:
#                                 dfIndex += 1
#                                 if m == df.Date[dfIndex].split('-')[1]:
#                                     xIndex.append(int(dfIndex))
#                                     break
#                     else:
#                         for y in year:
#                             for row in df.Date:
#                                 if y == df.Date[dfIndex].split('-')[0]:
#                                     xIndex.append(int(dfIndex))
#                                     break
#                                 dfIndex += 1
#                     print(xIndex)
#                     plt.title(files.replace('.csv', ''))
#                     plt.plot(df.Date, df.Price, label='Price')
#                     plt.plot(df.Date, df.Hold, label='Hold')
#                     if len(files.split('_')) != 2 and file.split('_') != 6:
#                         plt.scatter(df.Date, df.Hold,
#                                     c='darkorange', s=5, zorder=10)
#                     plt.xlabel('Date', fontsize=12, c='black')
#                     plt.ylabel('Price', fontsize=12, c='black')

#                     plt.xticks(xIndex, fontsize=9)
#                     plt.yticks(fontsize=9)
#                     plt.legend()

#                     plt.grid()
#                     plt.savefig(files.split('.')[0]+'.png',
#                                 dpi=fig.dpi, bbox_inches='tight')
#                     plt.cla()
#                     gc.collect()
#                     print('line chart for ' + files + ' created')
#         now += 1
#         os.chdir(oowd)
#     plt.close(fig)

os.chdir(owd)
index = []
i = 0
index.append(int(1))
with open('test_IRR.csv', 'rt') as f:
    file = csv.reader(f, delimiter=',')
    for row in file:
        if len(row) == 1 and i != 0:
            index.append(int(i))
            index.append(int(i+1))
        i += 1
    f.close()
index.append(int(i))
csvfile = open('test_IRR.csv', 'r').readlines()
os.chdir('IRR_split')

i = 0
j = 0
for i in range(len(index)):
    if i % 2 == 0:
        f = open(all_company[j]+'_IRR.csv', 'w+')
        f.write('Window,IRR\n')
        f.writelines(csvfile[index[i]:index[i+1]])
        f.close()
        j += 1
all_filename = os.listdir()

for file in all_filename:
    if file.split('.')[1] == 'csv':
        df = pd.read_csv(file)
        x = list(df.iloc[:, 0])
        y = list(df.iloc[:, 1])
        plt.grid(axis='y')
        plt.bar(x, y)
        print(file.split('.')[0])
        plt.savefig(file.split('.')[0]+'.png',
                    dpi=fig.dpi, bbox_inches='tight')
        plt.cla()
