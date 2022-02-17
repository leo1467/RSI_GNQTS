import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import glob
import os
import gc
import matplotlib.ticker as mtick

owd = os.getcwd()
all_company = os.listdir(os.chdir('result'))
all_company.sort()
oowd = os.getcwd()
file_extension = '.csv'
splitTargetFolder = ['/testBestHold']


def split_hold_period():
    for targetFolder in splitTargetFolder:
        for comName in all_company:
            if comName != '.DS_Store':
                os.chdir(comName+targetFolder)
                all_filename = [i for i in glob.glob(f"*{file_extension}")]
                for file in all_filename:
                    if (targetFolder == '/testBestHold' and len(file.split('_')) == 2) or (targetFolder == '/specify' and file.split('_')[0] == 'hold' and len(file.split('_')) == 6):
                        print('spliting ' + file)
                        year = [str(i) for i in range(2013, 2021)]
                        yIndex = []
                        yIN = -1
                        yIndex.append(int(1))
                        # with open(file, 'rt') as f:
                        #     reader = csv.reader(f, delimiter=',')
                        #     for y in year:
                        #         print(y)
                        #         for row in reader:  # 預想是每break一次都會從第一個row開始，但是好像會接續從break時的row接續下去？？不知道為什麼
                        #             print(row)
                        #             yIN += 1
                        #             if y == row[0].split('-')[0]:
                        #                 yIndex.append(int(yIN))
                        #                 yIndex.append(int(yIN))
                        #                 break
                        csvfile = open(''.join(file), 'r').readlines()
                        for y in year:
                            for row in range(len(csvfile)):
                                yIN += 1
                                if y == csvfile[yIN].split('-')[0]:
                                    yIndex.append(int(yIN))
                                    yIndex.append(int(yIN))
                                    break
                        yIndex.append(len(csvfile))
                        for i in range(len(yIndex)):
                            if i % 2 == 0:
                                if targetFolder == '/testBestHold':
                                    f = open(
                                        file.split('.')[0] + '_' + str(csvfile[yIndex[i]]).split('-')[0] + '_hold.csv', 'w+')
                                elif targetFolder == '/specify':
                                    f = open(
                                        file.split('.')[0] + '_' + str(csvfile[yIndex[i]]).split('-')[0] + '.csv', 'w+')
                                f.write('Date,Price,Hold\n')
                                f.writelines(csvfile[yIndex[i]:yIndex[i+1]])
                                f.close()
            os.chdir(oowd)


def draw_hold_period():
    fig = plt.figure(figsize=[16, 4.5], dpi=300)
    os.chdir(oowd)
    now = 0
    month = ['01', '02', '03', '04', '05', '06',
             '07', '08', '09', '10', '11', '12']
    year = [str(i) for i in range(2012, 2021)]
    for targetFolder in splitTargetFolder:
        for comName in all_company:
            if now >= 0 and comName != '.DS_Store':
                print(comName)
                os.chdir(comName+targetFolder)
                NewAll_filename = [i for i in glob.glob(f"*{file_extension}")]
                print(NewAll_filename)
                for files in NewAll_filename:
                    if files.split('_')[0] != 'RoR':
                        df = pd.read_csv(files)
                        xIndex = []
                        dfIndex = -1
                        if len(files.split('_')) == 4 or len(files.split('_')) == 7:
                            for m in month:
                                for row in df.Date:
                                    dfIndex += 1
                                    if m == df.Date[dfIndex].split('-')[1]:
                                        xIndex.append(int(dfIndex))
                                        break
                        else:
                            for y in year:
                                for row in df.Date:
                                    dfIndex += 1
                                    if y == df.Date[dfIndex].split('-')[0]:
                                        xIndex.append(int(dfIndex))
                                        break
                        plt.title(files.replace('.csv', ''))
                        plt.plot(df.Date, df.Price, label='Price')
                        plt.plot(df.Date, df.Hold, label='Hold')
                        if len(files.split('_')) != 2 and files.split('_') != 6:
                            plt.scatter(df.Date, df.Hold,
                                        c='darkorange', s=5, zorder=10)
                        plt.xlabel('Date', fontsize=12, c='black')
                        plt.ylabel('Price', fontsize=12, c='black')

                        plt.xticks(xIndex, fontsize=9)
                        plt.yticks(fontsize=9)
                        plt.legend()

                        plt.grid()
                        plt.savefig(files.split('.')[0]+'.png',
                                    dpi=fig.dpi, bbox_inches='tight')
                        plt.cla()
                        gc.collect()
                        print('line chart for ' + files + ' created')
            now += 1
            os.chdir(oowd)
        plt.close(fig)


def split_testIRR_draw(split):
    os.chdir(owd)
    # index = []
    # i = 0
    # index.append(int(1))
    # with open('test_IRR.csv', 'rt') as f:
    #     file = csv.reader(f, delimiter=',')
    #     for row in file:
    #         if row and i != 0:
    #             index.append(int(i))
    #             index.append(int(i+1))
    #         i += 1
    # index.append(int(i))
    csvfile = open('test_IRR.csv', 'r').readlines()
    index = []
    i = -1
    index.append(int(1))
    for row in range(len(csvfile)):
        i += 1
        if csvfile[row][0] == '=' and i != 0:
            index.append(int(i))
            index.append(int(i+1))
    index.append(int(i))
    os.chdir('IRR_split')
    i = 0
    j = 0
    for i in range(len(index)):
        if i % 2 == 0 and all_company[j] != '.DS_Store':
            if split == 0:  # 不分割csv
                f = open(all_company[j]+'_IRR.csv', 'w+')
                f.write('Window,GNQTS,Tradition\n')
                f.writelines(csvfile[index[i]:index[i+1]])
                f.close()
                j += 1
            elif split == 1:  # 分割成兩個csv
                f1 = open(all_company[j]+'_IRR_1.csv', 'w+')
                f1.write('Window,GNQTS,Tradition\n')
                f1.writelines(csvfile[index[i]:int(index[i] +
                                      (index[i+1] - index[i]) / 2)])
                f1.close()
                f2 = open(all_company[j]+'_IRR_2.csv', 'w+')
                f2.write('Window,GNQTS,Tradition\n')
                f2.writelines(
                    csvfile[int(index[i] + (index[i+1] - index[i]) / 2):index[i+1]])
                f2.close()
                j += 1

    all_filename = os.listdir()
    if split == 0:
        fig = plt.figure(figsize=[16, 4.5], dpi=300)  # 不分割圖片時的圖片size
    elif split == 1:
        fig = plt.figure(figsize=[16, 2.8], dpi=300)  # 分割圖片時的圖片size

    for index, file in enumerate(all_filename):
        if file.split('.')[1] == 'csv':
            df = pd.read_csv(file)
            width = 0.3
            x = np.arange(len(df.Window))
            # window = np.array(list(df.Window))

            clrGNQTS = ['r' if i == 'B&H' else 'steelblue' for i in df.Window]
            clrsTrandition = ['w' if i ==
                              'B&H' else 'darkorange' for i in df.Window]
            plt.bar(x, df.GNQTS, width, label='GNQTS', color=clrGNQTS)
            plt.bar(x+width, df.Tradition, width,
                    label='Tradition', color=clrsTrandition)
            plt.xticks(x + width / 2, df.Window, rotation=45, fontsize=10)

            plt.grid(axis='y')
            plt.title(file.split('_')[0])
            plt.legend()
            ax = plt.gca()
            ax.yaxis.set_major_formatter(mtick.PercentFormatter())  # 把座標變成%
            # 之後有空找兩張圖共用一個y axis
            # https://www.kite.com/python/answers/how-to-get-the-y-axis-range-of-a-plot-in-matplotlib-in-python
            # https://stackoverflow.com/questions/12608788/changing-the-tick-frequency-on-x-or-y-axis-in-matplotlib

            leg = ax.get_legend()
            leg.legendHandles[0].set_color('steelblue')
            leg.legendHandles[1].set_color('darkorange')

            lableClr1 = ["YYY2YYY", "YYY2YY", 'YYY2Y', "YYY2YH",
                         "YY2YY", "YY2YH", 'YY2Y', "YH2YH", 'YH2Y', 'Y2Y']
            lableClr2 = ["YYY2H", "YYY2Q", "YYY2M", 'YY2H', 'YY2Q',
                         'YY2M', "YH2H", "YH2Q", "YH2M", 'Y2H', 'Y2Q', 'Y2M']
            lableClr3 = ['H2H', 'H#', 'H2Q', 'Q2Q',
                         'Q#', 'H2M', 'Q2M', 'M2M', 'M#']
            lableclr4 = ['20D20', '20D15', '20D10', '20D5',
                         '15D15', '15D10', '15D5', '10D10', '10D5', '5D5']
            lableclr5 = ["4W4", "4W3", "4W2", "4W1",
                         "3W3", "3W2", "3W1", "2W2", "2W1", "1W1"]
            # lableClr6 = ["5D4", "5D3", "5D2", "4D3", "4D2", "3D2", "2D2"]
            transparent = 1
            for i in ax.get_xticklabels():
                txt = i.get_text()
                for tmp in lableClr1:
                    if txt == tmp:
                        plt.setp(i, bbox=dict(boxstyle="round", facecolor='gold',
                                              edgecolor='none', alpha=transparent))
                for tmp in lableClr2:
                    if txt == tmp:
                        plt.setp(i, bbox=dict(boxstyle="round", facecolor='limegreen',
                                              edgecolor='none', alpha=transparent))
                for tmp in lableClr3:
                    if txt == tmp:
                        plt.setp(i, bbox=dict(boxstyle="round", facecolor='r',
                                              edgecolor='none', alpha=transparent))
                for tmp in lableclr4:
                    if txt == tmp:
                        plt.setp(i, bbox=dict(boxstyle="round", facecolor='grey',
                                              edgecolor='none', alpha=transparent))
                for tmp in lableclr5:
                    if txt == tmp:
                        plt.setp(i, bbox=dict(boxstyle="round", facecolor='darkgoldenrod',
                                              edgecolor='none', alpha=transparent))
                # for tmp in lableClr6:
                #     if txt == tmp:
                #         plt.setp(i, backgroundcolor='white', size=6)
            # plt.setp(ax.get_xticklabels(), bbox=bbox)

            print(file.split('.')[0])
            plt.savefig(file.split('.')[0]+'.png',
                        dpi=fig.dpi, bbox_inches='tight')
            plt.cla()
            # print(index)
            # if index == 2:
            #     exit(0)


def draw_hold():
    fig = plt.figure(figsize=[16, 4.5], dpi=300)
    for company in all_company:
        os.chdir(company+'/testBestHold')
        all_filename = [i for i in glob.glob(f"*{file_extension}")]
        print(all_filename)
        for file in all_filename:
            df = pd.read_csv(file)
            
            yearIndexes = []
            year = 0
            for i, date in enumerate(df['Date']):
                nowYear = int(date.split('-')[0])
                if year != nowYear:
                    yearIndexes.append(i)
                    year = nowYear
            yearIndexes.append(df.index[-1])
            
            for yearIndex in range(len(yearIndexes)-1):
                newDf = df.iloc[yearIndexes[yearIndex]:yearIndexes[yearIndex+1]]
                newDf.reset_index(inplace = True, drop = True)
                
                ax = plt.gca()
                ax.plot(newDf['Date'],newDf['Price'],label='Price',color='g')
                ax.plot(newDf['Date'],newDf['Hold'],label='Hold',color='r')
                ax.scatter(newDf['Date'],newDf['buy'],c='darkorange', s=8, zorder=10,label='buy')
                ax.scatter(newDf['Date'],newDf['sell'],c='purple', s=8, zorder=10,label='sell')
                
                # buy = [i for i in newDf.index if not np.isnan(newDf.at[i,'buy'])]
                # sell = [i for i in newDf.index if not np.isnan(newDf.at[i,'sell'])]
                # ax.vlines(buy, color='darkorange', linestyle='-',alpha=0.5,label='buy',ymin=0,ymax=max(newDf['Price']))
                # ax.vlines(sell, color='purple', linestyle='-',alpha=0.5,label='sell',ymin=0,ymax=max(newDf['Price']))
                mIndex = []
                month = 0
                for i, date in enumerate(newDf['Date']):
                    nowMonth = int(date.split('-')[1])
                    if month != nowMonth:
                        mIndex.append(i)
                        month = nowMonth
                mIndex.append(newDf.index[-1])
                # ax.set_xticks(mIndex)
                plt.xticks(mIndex,fontsize=9)
                plt.yticks(fontsize=9)
                ax.legend()
                ax.grid()
                ax.set_xlabel('Date', fontsize=12, c='black')
                ax.set_ylabel('Price', fontsize=12, c='black')
                title = file.replace('.csv','_') + newDf.at[0,'Date'].split('-')[0] + '_Hold'
                print(title)
                ax.set_title(title)
                plt.savefig(title +'.png',dpi=fig.dpi, bbox_inches='tight')
                plt.clf()
        os.chdir(oowd)
        

if __name__ == '__main__':
    draw_hold()
    # split_testIRR_draw(1)
