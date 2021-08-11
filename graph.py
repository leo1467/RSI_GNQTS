import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import glob
import os
import csv
import gc
import matplotlib.dates as mdates

all_company = os.listdir(os.chdir('result'))

owd = os.getcwd()
for comName in all_company:
    print(comName)
    if comName != '.DS_Store':
        os.chdir(comName+'/testBestHold')
        file_extension = '.csv'
        all_filename = [i for i in glob.glob(f"*{file_extension}")]
        for file in all_filename:
            if len(file) < 14:
                print('spliting ' + file)
                year = ['2012', '2013', '2014', '2015',
                        '2016', '2017', '2018', '2019', '2020']
                yIndex = []
                yIN = 0
                yIndex.append(int(1))
                with open(file, 'rt') as f:
                    reader = csv.reader(f, delimiter=',')
                    for y in year:
                        for row in reader:
                            yIN += 1
                            if y == row[0].split('-')[0]:
                                yIndex.append(int(yIN)-1)
                                yIndex.append(int(yIN))
                                break
                    f.close()
                csvfile = open(''.join(file), 'r').readlines()
                yIndex.append(len(csvfile))
                for i in range(len(yIndex)):
                    if i % 2 == 0:
                        f = open(
                            comName + '_' + str(csvfile[yIndex[i]]).split('-')[0] + '_hold.csv', 'w+')
                        f.write('Date,Price,Hold\n')
                        f.writelines(csvfile[yIndex[i]:yIndex[i+1]])
                        f.close()
    os.chdir(owd)

now = 0
fig = plt.figure(figsize=[8, 5], dpi=600)
# ax = plt.subplot()
for comName in all_company:
    if now >= 0:
        os.chdir(comName+'/testBestHold')
        file_extension = '.csv'
        NewAll_filename = [i for i in glob.glob(f"*{file_extension}")]
        print(NewAll_filename)
        for files in NewAll_filename:
            df = pd.read_csv(files)
            month = ['01', '02', '03', '04',
                     '05', '06', '07', '08', '09', '10', '11', '12']
            year = ['2011', '2012', '2013', '2014', '2015',
                    '2016', '2017', '2018', '2019', '2020']
            xIndex = []
            dfIndex = 0
            if len(files) > 13:
                for m in month:
                    for row in df.Date:
                        if m == df.Date[dfIndex].split('-')[1]:
                            xIndex.append(int(dfIndex))
                            break
                        dfIndex += 1
            else:
                for y in year:
                    for row in df.Date:
                        if y == df.Date[dfIndex].split('-')[0]:
                            xIndex.append(int(dfIndex))
                            break
                        dfIndex += 1
            plt.title(files.replace('.csv', ''))
            plt.plot(df.Date, df.Price, label='Price')
            plt.plot(df.Date, df.Hold, label='Hold')
            if len(files) > 13:
                plt.scatter(df.Date, df.Hold, c='darkorange', s=5, zorder=10)
            plt.xlabel('Date', fontsize=8, c='black')
            plt.ylabel('Price', fontsize=8, c='black')

            plt.xticks(xIndex, fontsize=5, rotation=30)
            plt, plt.yticks(fontsize=5)
            plt.legend()

            plt.grid()
            plt.savefig(files.split('.')[0]+'.png',
                        dpi=fig.dpi, bbox_inches='tight')
            plt.cla()
            gc.collect()
            print('line chart for ' + files + ' created')
    now += 1
    os.chdir(owd)
plt.close(fig)
