//sliding windows
//finished on 2021/05/13
//
//
//
// #include <dirent.h>
// #include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "dirent.h"

using namespace std;
using namespace filesystem;

#define PARTICAL_AMOUNT 10
#define START 2  //股價csv裡的開始欄位
#define COL 4  //股價在第幾COLumn
#define TOTAL_CP_LV 10000000.0
#define PERIOD_BIT 8
#define OVERSOLD_BOUGHT_BIT 7

int mode = 4;  //0:train, 1:train_IRR, 2:test, 3:train_tradition, 4:cal_test_IRR, 5: tradition RSI, 6: fin_best_hold, 7:specify, 8:B&H, 9: del files, 10:make_RSI_table

double _delta = 0.003;
int _exp_times = 50;
int _generation = 1000;

string _train_start_date = "2009-01-02";
string _train_end_date = "2020-12-31";
string _test_start_y = "2011";
int _test_length = stoi(_train_end_date.substr(0, 4)) - stoi(_test_start_y) + 1;
string _sliding_windows[] = {/* "A2A", "Y2Y", "Y2H", "Y2Q", "Y2M", "H#", "H2H", "H2Q", "H2M", "Q#", "Q2Q", "Q2M", "M#", "M2M", "YY2Y" ,  */ "YH2Y"};

string _BH_company = "AAPL";
string _BH_start_day = "2011-01-03";  //also specify
string _BH_end_day = "2020-12-31";  //also specify
int _period = 1;
int _buySignal = 1;
int _sellSignal = 1;

string RSITable_path = "RSI_table";
string _price_path = "price";
string _output_path = "result";

struct prob {
    double period[PERIOD_BIT];
    double buying_signal[OVERSOLD_BOUGHT_BIT];  //oversold
    double selling_signal[OVERSOLD_BOUGHT_BIT];  //overbought
} prob_matrix;

struct partical {
    int period_bi[PERIOD_BIT];
    int buying_signal_bi[OVERSOLD_BOUGHT_BIT];  //oversold
    int selling_signal_bi[OVERSOLD_BOUGHT_BIT];  //overbought
    int period_dec;
    double buying_signal_dec;  //oversold
    double selling_signal_dec;  //overbought
    double RoR;
    double final_cp_lv;
    int trading_times;
} partical[PARTICAL_AMOUNT], the_best, Gbest, Gworst, Lbest, Lworst;

vector< vector< string > > read_data(string filename) {
    // cout << filename << endl;
    ifstream infile(filename, ios::in);
    string line;
    vector< vector< string > > data;
    if (!infile) {
        cout << "failed to open " + filename << endl;
        exit(1);
    }
    while (getline(infile, line)) {
        istringstream delime(line);
        string s;
        vector< string > line_data;
        while (getline(delime, s, ',')) {
            if (s != "\r") {
                line_data.push_back(s);
            }
        }
        data.push_back(line_data);
    }
    infile.close();
    return data;
}

vector< string > get_file(string path) {
    DIR* dir;
    struct dirent* ptr;
    vector< string > file_name;
    dir = opendir(path.c_str());
    while ((ptr = readdir(dir)) != NULL) {
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0 || strcmp(ptr->d_name, ".DS_Store") == 0) {
            continue;
        }
        file_name.push_back(ptr->d_name);
    }
    closedir(dir);
    sort(file_name.begin(), file_name.end());
    return file_name;
}

void ini_local() {
    for (int i = 0; i < PARTICAL_AMOUNT; i++) {
        for (int j = 0; j < PERIOD_BIT; j++) {
            partical[i].period_bi[j] = 0;
        }
        for (int j = 0; j < OVERSOLD_BOUGHT_BIT; j++) {
            partical[i].buying_signal_bi[j] = 0;
            partical[i].selling_signal_bi[j] = 0;
        }
        partical[i].period_dec = 0;
        partical[i].buying_signal_dec = 0;
        partical[i].selling_signal_dec = 0;
        partical[i].RoR = 0;
        partical[i].final_cp_lv = 0;
        partical[i].trading_times = 0;
    }

    for (int i = 0; i < PERIOD_BIT; i++) {
        Lbest.period_bi[i] = 0;
        Lworst.period_bi[i] = 0;
    }
    for (int i = 0; i < OVERSOLD_BOUGHT_BIT; i++) {
        Lbest.buying_signal_bi[i] = 0;
        Lbest.selling_signal_bi[i] = 0;
        Lworst.buying_signal_bi[i] = 0;
        Lworst.selling_signal_bi[i] = 0;
    }
    Lbest.period_dec = 0;
    Lbest.buying_signal_dec = 0;
    Lbest.selling_signal_dec = 0;
    Lbest.RoR = 0;
    Lbest.final_cp_lv = 0;
    Lbest.trading_times = 0;

    Lworst.period_dec = 0;
    Lworst.buying_signal_dec = 0;
    Lworst.selling_signal_dec = 0;
    Lworst.RoR = 100000;
    Lworst.final_cp_lv = 0;
    Lworst.trading_times = 0;
}

void ini_global() {
    for (int i = 0; i < PERIOD_BIT; i++) {
        Gbest.period_bi[i] = 0;
        Gworst.period_bi[i] = 0;
    }
    for (int i = 0; i < OVERSOLD_BOUGHT_BIT; i++) {
        Gbest.buying_signal_bi[i] = 0;
        Gbest.selling_signal_bi[i] = 0;
        Gworst.buying_signal_bi[i] = 0;
        Gworst.selling_signal_bi[i] = 0;
    }
    Gbest.period_dec = 0;
    Gbest.buying_signal_dec = 0;
    Gbest.selling_signal_dec = 0;
    Gbest.RoR = 0;
    Gbest.final_cp_lv = 0;
    Gbest.trading_times = 0;

    Gworst.period_dec = 0;
    Gworst.buying_signal_dec = 0;
    Gworst.selling_signal_dec = 0;
    Gworst.RoR = 100000;
    Gworst.final_cp_lv = 0;
    Gworst.trading_times = 0;
}

void ini_the_best() {
    for (int i = 0; i < PERIOD_BIT; i++) {
        the_best.period_bi[i] = 0;
    }
    for (int i = 0; i < OVERSOLD_BOUGHT_BIT; i++) {
        the_best.buying_signal_bi[i] = 0;
        the_best.selling_signal_bi[i] = 0;
    }
    the_best.period_dec = 0;
    the_best.buying_signal_dec = 0;
    the_best.selling_signal_dec = 0;
    the_best.RoR = 0;
    the_best.final_cp_lv = 0;
    the_best.trading_times = 0;
}

void ini_beta_matrix() {
    for (int i = 0; i < PERIOD_BIT; i++) {
        prob_matrix.period[i] = 0.5;
    }
    for (int i = 0; i < OVERSOLD_BOUGHT_BIT; i++) {
        prob_matrix.buying_signal[i] = 0.5;
        prob_matrix.selling_signal[i] = 0.5;
    }
}

void measure() {
    double r;
    for (int i = 0; i < PARTICAL_AMOUNT; i++) {
        for (int j = 0; j < PERIOD_BIT; j++) {
            r = rand();
            r = r / (double)RAND_MAX;
            if (r <= prob_matrix.period[j]) {
                partical[i].period_bi[j] = 1;
            }
            else {
                partical[i].period_bi[j] = 0;
            }
        }
        for (int j = 0; j < OVERSOLD_BOUGHT_BIT; j++) {
            r = rand();
            r = r / (double)RAND_MAX;
            if (r <= prob_matrix.buying_signal[j]) {
                partical[i].buying_signal_bi[j] = 1;
            }
            else {
                partical[i].buying_signal_bi[j] = 0;
            }
        }
        for (int j = 0; j < OVERSOLD_BOUGHT_BIT; j++) {
            r = rand();
            r = r / (double)RAND_MAX;
            if (r <= prob_matrix.selling_signal[j]) {
                partical[i].selling_signal_bi[j] = 1;
            }
            else {
                partical[i].selling_signal_bi[j] = 0;
            }
        }
    }
}

void bi_to_dec() {
    for (int i = 0; i < PARTICAL_AMOUNT; i++) {
        partical[i].period_dec = 0;
        partical[i].buying_signal_dec = 0;
        partical[i].selling_signal_dec = 0;
        for (int j = 0, k = PERIOD_BIT - 1; j < PERIOD_BIT; j++, k--) {
            partical[i].period_dec += pow(2, k) * partical[i].period_bi[j];
        }
        for (int j = 0, k = OVERSOLD_BOUGHT_BIT - 1; j < OVERSOLD_BOUGHT_BIT; j++, k--) {
            partical[i].buying_signal_dec += pow(2, k) * partical[i].buying_signal_bi[j];
            partical[i].selling_signal_dec += pow(2, k) * partical[i].selling_signal_bi[j];
        }
        partical[i].period_dec++;
    }
}

void update_local(int partical_num) {
    if (partical[partical_num].RoR > Lbest.RoR) {
        for (int i = 0; i < PERIOD_BIT; i++) {
            Lbest.period_bi[i] = partical[partical_num].period_bi[i];
        }
        for (int i = 0; i < OVERSOLD_BOUGHT_BIT; i++) {
            Lbest.buying_signal_bi[i] = partical[partical_num].buying_signal_bi[i];
            Lbest.selling_signal_bi[i] = partical[partical_num].selling_signal_bi[i];
        }
        Lbest.period_dec = partical[partical_num].period_dec;
        Lbest.buying_signal_dec = partical[partical_num].buying_signal_dec;
        Lbest.selling_signal_dec = partical[partical_num].selling_signal_dec;
        Lbest.RoR = partical[partical_num].RoR;
        Lbest.final_cp_lv = partical[partical_num].final_cp_lv;
        Lbest.trading_times = partical[partical_num].trading_times;
    }

    if (partical[partical_num].RoR < Lworst.RoR) {
        for (int i = 0; i < PERIOD_BIT; i++) {
            Lworst.period_bi[i] = partical[partical_num].period_bi[i];
        }
        for (int i = 0; i < OVERSOLD_BOUGHT_BIT; i++) {
            Lworst.buying_signal_bi[i] = partical[partical_num].buying_signal_bi[i];
            Lworst.selling_signal_bi[i] = partical[partical_num].selling_signal_bi[i];
        }
        Lworst.period_dec = partical[partical_num].period_dec;
        Lworst.buying_signal_dec = partical[partical_num].buying_signal_dec;
        Lworst.selling_signal_dec = partical[partical_num].selling_signal_dec;
        Lworst.RoR = partical[partical_num].RoR;
        Lworst.final_cp_lv = partical[partical_num].final_cp_lv;
        Lworst.trading_times = partical[partical_num].trading_times;
    }
}

void update_global() {
    if (Lbest.RoR > Gbest.RoR) {
        for (int i = 0; i < PERIOD_BIT; i++) {
            Gbest.period_bi[i] = Lbest.period_bi[i];
        }
        for (int i = 0; i < OVERSOLD_BOUGHT_BIT; i++) {
            Gbest.buying_signal_bi[i] = Lbest.buying_signal_bi[i];
            Gbest.selling_signal_bi[i] = Lbest.selling_signal_bi[i];
        }
        Gbest.period_dec = Lbest.period_dec;
        Gbest.buying_signal_dec = Lbest.buying_signal_dec;
        Gbest.selling_signal_dec = Lbest.selling_signal_dec;
        Gbest.RoR = Lbest.RoR;
        Gbest.final_cp_lv = Lbest.final_cp_lv;
        Gbest.trading_times = Lbest.trading_times;
    }
    //===============================================================================
    if (Gbest.RoR != 0) {
        for (int i = 0; i < PERIOD_BIT; i++) {
            if (Gbest.period_bi[i] == 1 && Lworst.period_bi[i] == 0 && prob_matrix.period[i] < 0.5) {
                prob_matrix.period[i] = 1.0 - prob_matrix.period[i];
            }
            if (Gbest.period_bi[i] == 0 && Lworst.period_bi[i] == 1 && prob_matrix.period[i] > 0.5) {
                prob_matrix.period[i] = 1.0 - prob_matrix.period[i];
            }
            if (Gbest.period_bi[i] == 1) {
                prob_matrix.period[i] += _delta;
            }
            if (Lworst.period_bi[i] == 1) {
                prob_matrix.period[i] -= _delta;
            }
        }
        for (int i = 0; i < OVERSOLD_BOUGHT_BIT; i++) {
            if (Gbest.buying_signal_bi[i] == 1 && Lworst.buying_signal_bi[i] == 0 && prob_matrix.buying_signal[i] < 0.5) {
                prob_matrix.buying_signal[i] = 1.0 - prob_matrix.buying_signal[i];
            }
            if (Gbest.buying_signal_bi[i] == 0 && Lworst.buying_signal_bi[i] == 1 && prob_matrix.buying_signal[i] > 0.5) {
                prob_matrix.buying_signal[i] = 1.0 - prob_matrix.buying_signal[i];
            }
            if (Gbest.selling_signal_bi[i] == 1 && Lworst.selling_signal_bi[i] == 0 && prob_matrix.selling_signal[i] < 0.5) {
                prob_matrix.selling_signal[i] = 1.0 - prob_matrix.selling_signal[i];
            }
            if (Gbest.selling_signal_bi[i] == 0 && Lworst.selling_signal_bi[i] == 1 && prob_matrix.selling_signal[i] > 0.5) {
                prob_matrix.selling_signal[i] = 1.0 - prob_matrix.selling_signal[i];
            }
            if (Gbest.buying_signal_bi[i] == 1) {
                prob_matrix.buying_signal[i] += _delta;
            }
            if (Gbest.selling_signal_bi[i] == 1) {
                prob_matrix.selling_signal[i] += _delta;
            }
            if (Lworst.buying_signal_bi[i] == 1) {
                prob_matrix.buying_signal[i] -= _delta;
            }
            if (Lworst.selling_signal_bi[i] == 1) {
                prob_matrix.selling_signal[i] -= _delta;
            }
        }
    }
    //===============================================================================
}

void update_the_best() {
    if (the_best.RoR < Gbest.RoR) {
        for (int i = 0; i < PERIOD_BIT; i++) {
            the_best.period_bi[i] = Gbest.period_bi[i];
        }
        for (int i = 0; i < OVERSOLD_BOUGHT_BIT; i++) {
            the_best.buying_signal_bi[i] = Gbest.buying_signal_bi[i];
            the_best.selling_signal_bi[i] = Gbest.selling_signal_bi[i];
        }
        the_best.period_dec = Gbest.period_dec;
        the_best.buying_signal_dec = Gbest.buying_signal_dec;
        the_best.selling_signal_dec = Gbest.selling_signal_dec;
        the_best.RoR = Gbest.RoR;
        the_best.final_cp_lv = Gbest.final_cp_lv;
        the_best.trading_times = Gbest.trading_times;
    }
}

int find_train_end(int table_size, string mon, int& train_end_row, int to_jump, string* daysTable) {
    int skip = 0;
    for (int i = table_size - 1; i >= 0; i--) {
        if (daysTable[i].substr(5, 2) != mon) {
            skip++;
            mon = daysTable[i].substr(5, 2);
            if (skip == to_jump) {
                train_end_row = i;
                break;
            }
            i -= 15;
        }
    }
    return skip;
}

int find_train_start(int test_s_row, string mon, int& train_start_row, string& start, string& end, int to_jump, string* daysTable) {
    int skip = 0;
    if (to_jump == 13) {  //給Y2Y用
        train_start_row = 0;
        start = daysTable[0].substr(5, 2);
        end = daysTable[test_s_row].substr(5, 2);
    }
    else {
        for (int i = test_s_row; i >= -1; i--) {
            if (daysTable[i].substr(5, 2) != mon) {
                skip++;
                mon = daysTable[i].substr(5, 2);
                if (skip == to_jump) {
                    train_start_row = i + 1;
                    start = daysTable[i + 1].substr(5, 2);
                    end = daysTable[test_s_row].substr(5, 2);
                    break;
                }
                i -= 15;
            }
        }
    }
    return to_jump;
}

void find_window_start_end(int table_size, string windowUse, int sliding_type_int, string* daysTable, vector< int >& interval_table) {
    string M[] = {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12"};
    int test_s_row = 0;  //記錄測試期開始row
    for (int i = 230; i < 300; i++) {  //找出測試期開始的row
        if (daysTable[i].substr(0, 4) == _test_start_y) {
            test_s_row = i;
            break;
        }
    }
    int train_start_row = 0;  //記錄訓練期開始的row
    int train_end_row = 0;  //記錄訓練期結束的row
    if (sliding_type_int == 2) {  //判斷year-on-year
        train_start_row = 0;
        string train_end_y = to_string(stoi(_train_end_date.substr(0, 4)) - 1);
        for (int i = table_size - 230; i > 0; i--) {
            if (daysTable[i].substr(0, 4) == train_end_y) {
                train_end_row = i;
                break;
            }
        }
        switch (windowUse[0]) {  //做H*
            case 'H': {
                interval_table.push_back(train_start_row);
                string end_H = M[6];
                for (int i = 97, j = 6; i < train_end_row; i++) {
                    if (daysTable[i].substr(5, 2) == end_H) {
                        interval_table.push_back(i - 1);
                        interval_table.push_back(i);
                        end_H = M[(j += 6) % 12];
                        i += 97;
                    }
                }
                interval_table.push_back(train_end_row);
                break;
            }
            case 'Q': {  //做Q*
                interval_table.push_back(train_start_row);
                string end_Q = M[3];
                for (int i = 55, j = 3; i < train_end_row; i++) {
                    if (daysTable[i].substr(5, 2) == end_Q) {
                        interval_table.push_back(i - 1);
                        interval_table.push_back(i);
                        end_Q = M[(j += 3) % 12];
                        i += 55;
                    }
                }
                interval_table.push_back(train_end_row);
                break;
            }
            case 'M': {  //做M*
                interval_table.push_back(train_start_row);
                string end_H = M[1];
                for (int i = 15, j = 1; i < train_end_row; i++) {
                    if (daysTable[i].substr(5, 2) == end_H) {
                        interval_table.push_back(i - 1);
                        interval_table.push_back(i);
                        end_H = M[++j % 12];
                        i += 15;
                    }
                }
                interval_table.push_back(train_end_row);
                break;
            }
        }
    }
    else {
        string mon = _train_start_date.substr(5, 2);  //記錄目前iterate到什麼月份
        string start;  //記錄sliding window開始月份
        string end;  //記錄sliding window結束月份
        int s_jump_e = 0;  //從period頭要找period尾
        int e_jump_s = 0;  //從period尾要找下一個period頭
        int skip = 0;  //M[]要跳幾個月份
        switch (windowUse[0]) {  //看是什麼開頭，找出第一個訓練開始的row以及開始月份及結束月份
            case 'Y': {
                s_jump_e = find_train_start(test_s_row, mon, train_start_row, start, end, 13, daysTable) - 1;
                break;
            }
            case 'H': {
                s_jump_e = find_train_start(test_s_row, mon, train_start_row, start, end, 7, daysTable) - 1;
                break;
            }
            case 'Q': {
                s_jump_e = find_train_start(test_s_row, mon, train_start_row, start, end, 4, daysTable) - 1;
                break;
            }
            case 'M': {
                s_jump_e = find_train_start(test_s_row, mon, train_start_row, start, end, 2, daysTable) - 1;
                break;
            }
        }
        switch (windowUse[2]) {  //看是什麼結尾，找出最後一個訓練開始的row
            case 'Y': {
                skip = find_train_end(table_size, mon, train_end_row, 13, daysTable) - 1;
                break;
            }
            case 'H': {
                skip = find_train_end(table_size, mon, train_end_row, 7, daysTable) - 1;
                break;
            }
            case 'Q': {
                skip = find_train_end(table_size, mon, train_end_row, 4, daysTable) - 1;
                break;
            }
            case 'M': {
                skip = find_train_end(table_size, mon, train_end_row, 2, daysTable) - 1;
                break;
            }
        }
        int train_s_M = 0, train_e_M = 12;  //記錄第一個訓練期開頭及結尾在M[]的index
        for (int i = 0; i < 12; i++) {
            if (start == M[i]) {
                train_s_M = i;
                break;
            }
        }
        if (windowUse[0] == windowUse[2]) {  //判斷從period尾要找下一個period頭需要跳多少
            e_jump_s = 1;
        }
        else if (windowUse[2] == 'M') {
            e_jump_s = s_jump_e - 1;
        }
        else if (windowUse[2] == 'Q') {
            e_jump_s = s_jump_e - 3;
        }
        else {
            e_jump_s = 6;
        }
        interval_table.push_back(train_start_row);
        for (int i = train_start_row + s_jump_e * 18, k = 1; i < train_end_row; i++) {
            if (k % 2 == 1 && daysTable[i].substr(5, 2) == end) {
                interval_table.push_back(i - 1);
                start = M[(train_s_M += skip) % 12];
                end = M[(train_e_M += skip) % 12];
                i -= e_jump_s * 24;
                if (i < 0) {
                    i = 0;
                }
                k++;
            }
            else if (k % 2 == 0 && daysTable[i].substr(5, 2) == start) {
                interval_table.push_back(i);
                i += s_jump_e * 18;
                k++;
            }
        }
        interval_table.push_back(train_end_row);
    }
}

void find_YY2Y_window(int tableSize, string windowUse, int slideType, string* daysTable, vector< int >& intervalTable) {
    int testStartRow = -1;
    for (int i = 0; i < tableSize; i++) {
        if (daysTable[i].substr(0, 4) == _test_start_y) {
            testStartRow = i;
            break;
        }
    }
    if (testStartRow == -1) {
        cout << "can not find " + windowUse + " testStartRow" << endl;
        exit(1);
    }
    int trainStartYear = stoi(_train_start_date.substr(0, 4));
    int trainEndYear = trainStartYear + 2;
    vector< int > startRow;
    vector< int > endRow;
    for (int i = 0; startRow.size() < _test_length; i++) {
        if (trainStartYear == stoi(daysTable[i].substr(0, 4))) {
            startRow.push_back(i);
            trainStartYear++;
        }
    }
    for (int i = 0; endRow.size() < _test_length; i++) {
        if (trainEndYear == stoi(daysTable[i].substr(0, 4))) {
            endRow.push_back(i - 1);
            trainEndYear++;
        }
    }
    for (int i = 0; i < startRow.size(); i++) {
        intervalTable.push_back(startRow[i]);
        intervalTable.push_back(endRow[i]);
        // cout << daysTable[startRow[i]] << endl;
    }
}

void find_YH2Y_window(int tableSize, string windowUse, int slideType, string* daysTable, vector< int >& intervalTable) {
    int testStartRow = -1;
    for (int i = 0; i < tableSize; i++) {
        if (daysTable[i].substr(5, 2) == "07") {
            testStartRow = i;
            break;
        }
    }
    if (testStartRow == -1) {
        cout << "can not find " + windowUse + " testStartRow" << endl;
        exit(1);
    }
    int trainStartYear = stoi(_train_start_date.substr(0, 4));
    int trainEndYear = trainStartYear + 2;
    vector< int > startRow;
    vector< int > endRow;
    for (int i = 0; startRow.size() < _test_length; i++) {
        if (trainStartYear == stoi(daysTable[i].substr(0, 4)) && daysTable[i].substr(5, 2) == "07") {
            startRow.push_back(i);
            trainStartYear++;
            i += 30;
        }
    }
    for (int i = 0; endRow.size() < _test_length; i++) {
        if (trainEndYear == stoi(daysTable[i].substr(0, 4))) {
            endRow.push_back(i - 1);
            trainEndYear++;
        }
    }
    for (int i = 0; i < startRow.size(); i++) {
        intervalTable.push_back(startRow[i]);
        intervalTable.push_back(endRow[i]);
        // cout << daysTable[startRow[i]] << endl;
    }
}

void find_interval(int table_size, int slide, string* daysTable, vector< int >& interval_table) {  //將A2A跟其他分開
    cout << "looking for interval " + _sliding_windows[slide] << endl;
    if (_sliding_windows[slide] == "A2A") {  //直接給A2A的起始與結束row
        interval_table.push_back(0);
        interval_table.push_back(table_size - 1);
    }
    else if (_sliding_windows[slide] == "YY2Y") {
        find_YY2Y_window(table_size, _sliding_windows[slide], (int)_sliding_windows[slide].size(), daysTable, interval_table);
    }
    else if (_sliding_windows[slide] == "YH2Y") {
        find_YH2Y_window(table_size, _sliding_windows[slide], (int)_sliding_windows[slide].size(), daysTable, interval_table);
    }
    else {  //開始找普通滑動視窗的起始與結束
        find_window_start_end(table_size, _sliding_windows[slide], (int)_sliding_windows[slide].size(), daysTable, interval_table);
    }
    for (int i = 0; i < interval_table.size(); i += 2) {
        cout << daysTable[interval_table[i]] + "~" + daysTable[interval_table[i + 1]] << endl;
    }
}

void cal_train_RoR(int interval_index, int& earlestGen, int gen, vector< int > interval_table, double** RSITable, double* priceTable) {
    int stock_held = 0;
    double remain = TOTAL_CP_LV;
    int flag = 0;  //記錄是否有交易過
    for (int partical_num = 0; partical_num < PARTICAL_AMOUNT; partical_num++) {
        stock_held = 0;
        remain = TOTAL_CP_LV;
        flag = 0;
        // ===============================================================開始計算報酬率
        for (int i = interval_table[interval_index]; i <= interval_table[interval_index + 1]; i++) {
            if (RSITable[i][partical[partical_num].period_dec] <= partical[partical_num].buying_signal_dec && stock_held == 0) {  //買入訊號出現且無持股
                stock_held = remain / priceTable[i];
                remain = remain - priceTable[i] * stock_held;
                flag = 1;
                partical[partical_num].trading_times++;
                if (i == interval_table[interval_index + 1]) {
                    partical[partical_num].trading_times--;
                }
            }
            else if (RSITable[i][partical[partical_num].period_dec] >= partical[partical_num].selling_signal_dec && stock_held != 0) {  //賣出訊號出現且有持股
                remain += (double)stock_held * priceTable[i];
                stock_held = 0;
            }
        }
        if (flag == 0) {  //沒有交易過
            partical[partical_num].RoR = 0;
            partical[partical_num].final_cp_lv = 0;
        }
        else if (stock_held != 0) {  //有交易過且有持股
            remain += stock_held * priceTable[interval_table[interval_index + 1]];
            partical[partical_num].RoR = ((remain - TOTAL_CP_LV) / TOTAL_CP_LV) * 100;
            partical[partical_num].final_cp_lv = remain;
        }
        else {  //有交易過但沒有持股
            partical[partical_num].RoR = ((remain - TOTAL_CP_LV) / TOTAL_CP_LV) * 100;
            partical[partical_num].final_cp_lv = remain;
        }
        // ===============================================================報酬率計算結束
        update_local(partical_num);
    }
    if (Gbest.RoR < Lbest.RoR) {
        earlestGen = gen;
    }
    update_global();
}

void cal(int interval_index, int& earlestGen, vector< int > interval_table, double** RSITable, double* priceTable) {
    ini_global();
    ini_beta_matrix();
    for (int gen = 0; gen < _generation; gen++) {
        ini_local();
        measure();
        bi_to_dec();
        cal_train_RoR(interval_index, earlestGen, gen, interval_table, RSITable, priceTable);
    }
    // cout << the_best.RoR << "%" << endl;
}

void output(int interval_index, int slide, string company, int earlestExp, int earlestGen, vector< int > interval_table, string* daysTable, double** RSITable, double* priceTable) {
    ofstream data;
    data.open(_output_path + "/" + company + "/train/" + _sliding_windows[slide] + "/" + daysTable[interval_table[interval_index]] + "_" + daysTable[interval_table[interval_index + 1]] + ".csv");
    data << "generation," << _generation << endl;
    data << "Partical amount," << PARTICAL_AMOUNT << endl;
    data << "delta," << _delta << endl;
    data << "EXP times," << _exp_times << endl;
    data << endl;
    data << fixed << setprecision(0) << "Initial capital," << TOTAL_CP_LV << endl;
    if (the_best.RoR == 0) {
        data << "Final capital," << TOTAL_CP_LV << endl;
        data << "Final return,0" << endl;
    }
    else {
        data << "Final capital," << the_best.final_cp_lv << endl;
        data << "Final return," << the_best.final_cp_lv - TOTAL_CP_LV << endl;
    }
    data << endl;
    data << "Period," << the_best.period_dec << endl;
    data << "Buying signal," << fixed << setprecision(10) << the_best.buying_signal_dec << endl;
    data << "Selling signal," << fixed << setprecision(10) << the_best.selling_signal_dec << endl;
    data << "Trading times," << the_best.trading_times << endl;
    data << "Rate of return," << fixed << setprecision(10) << the_best.RoR << "%" << endl;
    data << endl;
    data << "first best exp," << earlestExp << endl;
    data << "first best gen," << earlestGen << endl;
    data << "best times," << endl;
    data << endl;
    data << "Trading record,Date,Price,RSI,Stock held,Remain,Capital Lv" << endl;
    // ===============================================================開始輸出每次交易細節
    if (the_best.RoR != 0) {
        int stock_held = 0;
        double remain = TOTAL_CP_LV;
        int flag = 0;
        int throw_out = 0;
        for (int i = interval_table[interval_index]; i <= interval_table[interval_index + 1]; i++) {
            if (RSITable[i][the_best.period_dec] <= the_best.buying_signal_dec && stock_held == 0) {  //買入訊號出現且無持股
                stock_held = remain / priceTable[i];
                remain = remain - priceTable[i] * stock_held;
                if (i != interval_table[interval_index + 1]) {
                    data << "Buy," + daysTable[i] + "," << priceTable[i] << "," << RSITable[i][the_best.period_dec] << "," << stock_held;
                    data << "," << remain << "," << remain + priceTable[i] * stock_held << endl;
                }
                if (i == interval_table[interval_index + 1]) {
                    flag = 1;
                }
            }
            else if (RSITable[i][the_best.period_dec] >= the_best.selling_signal_dec && stock_held != 0) {  //賣出訊號出現且有持股
                remain += (double)stock_held * priceTable[i];
                stock_held = 0;
                data << "Sell," + daysTable[i] + "," << priceTable[i] << "," << RSITable[i][the_best.period_dec] << "," << stock_held;
                data << "," << remain << "," << remain + priceTable[i] * stock_held << endl;
                data << endl;
            }
            throw_out = i;
        }
        if (stock_held != 0) {  //有交易過且有持股
            remain += stock_held * priceTable[throw_out];
            stock_held = 0;
            if (flag != 1) {
                data << "Sell," + daysTable[throw_out] + "," << priceTable[throw_out] << "," << RSITable[throw_out][the_best.period_dec] << "," << stock_held;
                data << "," << remain << "," << remain + priceTable[throw_out] * stock_held << endl;
                data << endl;
            }
        }
    }
    // ===============================================================結束輸出每次交易細節
    data.close();
}

string* store_days_to_arr(string RSI_table_path, int& totalDays) {
    vector< vector< string > > RSI_table_in = read_data(RSI_table_path);
    totalDays = (int)RSI_table_in.size();
    string* daysTable = new string[totalDays];
    for (int i = 0; i < totalDays; i++) {
        daysTable[i] = RSI_table_in[i][0];
    }
    return daysTable;
}

double* store_price_to_arr(string stock_file_path, int totalDays) {
    vector< vector< string > > stock_price_in = read_data(stock_file_path);
    int price_size = (int)stock_price_in.size();
    int startingRow = 0;
    int ending_row = 0;
    for (int i = 0; i < price_size; i++) {
        if (stock_price_in[i][0] == _train_start_date) {
            startingRow = i;
            for (; i < price_size; i++) {
                if (stock_price_in[i][0] == _train_end_date) {
                    ending_row = i;
                    break;
                }
            }
            break;
        }
    }
    double* priceTable = new double[totalDays];
    for (int i = 0, j = startingRow; j <= ending_row; i++, j++) {
        priceTable[i] = stod(stock_price_in[j][COL]);
    }
    return priceTable;
}

double** store_RSI_table_to_arr(string RSI_table_path, int totalDays) {
    vector< vector< string > > RSI_table_in = read_data(RSI_table_path);
    double** RSITable = new double*[totalDays];
    for (int i = 0; i < totalDays; i++) {
        RSITable[i] = new double[257];
        for (int j = 1; j < 257; j++) {
            RSITable[i][j] = stod(RSI_table_in[i][j]);
        }
    }
    return RSITable;
}

void start_train() {
    vector< string > RSI_file = get_file(RSITable_path);  //get RSI table
    vector< string > stock_file = get_file(_price_path);  //get stock price
    vector< string > company = get_file(_output_path);
    int companyNum = (int)company.size();
    for (int company_index = 0; company_index < companyNum; company_index++) {
        cout << company[company_index] << endl;
        cout << RSI_file[company_index] << endl;
        cout << stock_file[company_index] << endl;
        if (stoi(_train_start_date.substr(0, 4)) > 2009 || company[company_index] != "V") {
            cout << "===========================train " << company[company_index] << endl;
            int totalDays = 0;
            string* daysTable = store_days_to_arr(RSITable_path + "/" + RSI_file[company_index], totalDays);  //記錄開始日期到結束日期
            double* priceTable = store_price_to_arr(_price_path + "/" + stock_file[company_index], totalDays);  //記錄開始日期的股價到結束日期的股價
            double** RSITable = store_RSI_table_to_arr(RSITable_path + "/" + RSI_file[company_index], totalDays);  //記錄一間公司開始日期到結束日期1~256的RSI
            int windowNum = sizeof(_sliding_windows) / sizeof(_sliding_windows[0]);
            for (int windowIndex = 0; windowIndex < windowNum; windowIndex++) {
                srand(343);
                vector< int > interval_table;  //記錄視窗區間
                find_interval(totalDays, windowIndex, daysTable, interval_table);
                int interval_cnt = (int)interval_table.size();
                for (int interval_index = 0; interval_index < interval_cnt; interval_index += 2) {
                    int earlestExp = 0;
                    int earlestGen = 0;
                    int theBestGen = 0;
                    int bestTimes = 0;
                    cout << "===" + daysTable[interval_table[interval_index]] + "~" + daysTable[interval_table[interval_index + 1]] + "===" << endl;
                    ini_the_best();
                    for (int exp = 0; exp < _exp_times; exp++) {
                        // cout << "exp: " << exp + 1 << "   ";
                        cal(interval_index, earlestGen, interval_table, RSITable, priceTable);
                        if (the_best.RoR < Gbest.RoR) {
                            earlestExp = exp;
                            theBestGen = earlestGen;
                        }
                        update_the_best();
                        // cout << Gbest.RoR << "%" << endl;
                    }
                    output(interval_index, windowIndex, company[company_index], earlestExp + 1, theBestGen + 1, interval_table, daysTable, RSITable, priceTable);
                    cout << the_best.RoR << "%" << endl;
                }
                vector< int >().swap(interval_table);
            }
            delete[] daysTable;
            delete[] priceTable;
            for (int i = 0; i < totalDays; i++) {
                delete[] RSITable[i];
            }
            delete[] RSITable;
        }
    }
}

void output_test_file(string RoROutputPath, string startDate, string endDate, int period, double buySignal, double sellSignal, int tradeNum, double returnRate, vector< string > tradeReord) {
    ofstream test;
    if (mode == 2 || mode == 3) {
        test.open(RoROutputPath + "/" + startDate + "_" + endDate + ".csv");
    }
    else {
        test.open(RoROutputPath + "/RoR_" + to_string(period) + "_" + to_string((int)buySignal) + "_" + to_string((int)sellSignal) + "_" + startDate + "_" + endDate + ".csv");
    }
    test << "generation," << _generation << endl;
    test << "Partical amount," << PARTICAL_AMOUNT << endl;
    test << "delta," << _delta << endl;
    test << "EXP times," << _exp_times << endl;
    test << endl;
    test << "Initial capital," << TOTAL_CP_LV << endl;
    if (returnRate == 0) {
        test << "Final capital," << TOTAL_CP_LV << endl;
        test << "Final return,0" << endl;
    }
    else {
        test << "Final capital," << TOTAL_CP_LV * (returnRate + 1) << endl;
        test << "Final return," << TOTAL_CP_LV * returnRate << endl;
    }
    test << endl;
    test << "Period," << period << endl;
    test << "Buying signal," << fixed << setprecision(10) << buySignal << endl;
    test << "Selling signal," << fixed << setprecision(10) << sellSignal << endl;
    test << "Trading times," << tradeNum << endl;
    test << "Rate of return," << fixed << setprecision(10) << returnRate * 100 << "%" << endl;
    test << endl;
    test << "first best exp," << endl;
    test << "first best gen," << endl;
    test << "best times," << endl;
    test << endl;
    test << "Trading record,Date,Price,RSI,Stock held,Remain,Capital Lv" << endl;
    for (int i = 0; i < tradeReord.size(); i++) {
        test << tradeReord[i] << endl;
        if (i % 2 == 1) {
            test << endl;
        }
    }
    test.close();
}

void cal_test_RoR(string* daysTable, double* priceTable, double** RSITable, string startDate, string endDate, int period, int buySignal, int sellSignal, int totalDays, string RoROutputPath, ofstream& holdPeriod) {
    int startingRow = 0;
    for (int i = 0; i < totalDays; i++) {
        if (daysTable[i] == startDate) {
            startingRow = i;
            break;
        }
    }
    int endingRow = 0;
    for (int i = startingRow; i < totalDays; i++) {
        if (daysTable[i] == endDate) {
            endingRow = i;
            break;
        }
    }
    // cout << daysTable[startingRow] << endl;
    // cout << daysTable[endingRow] << endl;
    int stockHold = 0;
    double remain = TOTAL_CP_LV;
    // int flag = 0;  //記錄手上有錢還是有股票
    int sellNum = 0;
    int buyNum = 0;
    double returnRate = 0;
    vector< string > tradeRecord;
    for (int i = startingRow; i <= endingRow; i++) {
        holdPeriod << daysTable[i] + "," << priceTable[i] << ",";
        if (period != 0 && RSITable[i][period] <= buySignal && stockHold == 0 && i < endingRow) {  //買入訊號出現且無持股
            buyNum++;
            stockHold = remain / priceTable[i];
            remain -= (double)stockHold * priceTable[i];
            tradeRecord.push_back("buy," + daysTable[i] + "," + to_string(priceTable[i]) + "," + to_string(RSITable[i][period]) + "," + to_string(stockHold) + "," + to_string(remain) + "," + to_string(remain + priceTable[i] * stockHold));
            holdPeriod << priceTable[i] << endl;
        }
        else if (period != 0 && ((RSITable[i][period] >= sellSignal && stockHold != 0) || (stockHold != 0 && i == endingRow))) {  //賣出訊號出現且有持股
            holdPeriod << priceTable[i] << endl;
            sellNum++;
            remain += (double)stockHold * priceTable[i];
            stockHold = 0;
            tradeRecord.push_back("sell," + daysTable[i] + "," + to_string(priceTable[i]) + "," + to_string(RSITable[i][period]) + "," + to_string(stockHold) + "," + to_string(remain) + "," + to_string(remain + priceTable[i] * stockHold));
        }
        else if (stockHold != 0) {
            holdPeriod << priceTable[i] << endl;
        }
        else if (stockHold == 0) {
            holdPeriod << endl;
        }
    }
    returnRate = (remain - TOTAL_CP_LV) / TOTAL_CP_LV;
    output_test_file(RoROutputPath, startDate, endDate, period, buySignal, sellSignal, sellNum, returnRate, tradeRecord);
}

vector< string > find_test_interval(char interval, int totalDays, string* daysTable) {
    vector< string > testInterval;
    int testStartRow = 0;
    for (int i = 0; i < totalDays; i++) {
        if (daysTable[i].substr(0, 4) == _test_start_y) {
            testStartRow = i;
            break;
        }
    }
    string month[] = {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12"};
    testInterval.push_back(daysTable[testStartRow]);
    int findPeriodEnd = 1;  //表示現在要記錄開始日期還是結束日期
    switch (interval) {
        case 'Y': {
            int nextPeriodStartRow = testStartRow;
            for (; nextPeriodStartRow < totalDays;) {
                if (findPeriodEnd == 1) {
                    string periodEndDate = daysTable[nextPeriodStartRow].substr(0, 4);
                    for (; nextPeriodStartRow < totalDays; nextPeriodStartRow++) {
                        if (daysTable[nextPeriodStartRow].substr(0, 4) != periodEndDate) {
                            testInterval.push_back(daysTable[nextPeriodStartRow - 1]);
                            findPeriodEnd = 0;
                            break;
                        }
                    }
                }
                else {
                    testInterval.push_back(daysTable[nextPeriodStartRow]);
                    findPeriodEnd = 1;
                }
            }
            testInterval.push_back(daysTable[nextPeriodStartRow - 1]);
            break;
        }
        case 'H': {
            int nextPeriodStartIndex = 6;
            int nextPeriodStartRow = testStartRow;
            for (; nextPeriodStartRow < totalDays;) {
                if (findPeriodEnd == 1) {
                    for (; nextPeriodStartRow < totalDays; nextPeriodStartRow++) {
                        if (daysTable[nextPeriodStartRow].substr(5, 2) == month[nextPeriodStartIndex]) {
                            testInterval.push_back(daysTable[nextPeriodStartRow - 1]);
                            findPeriodEnd = 0;
                            break;
                        }
                    }
                }
                else {
                    testInterval.push_back(daysTable[nextPeriodStartRow]);
                    nextPeriodStartIndex = (nextPeriodStartIndex + 6) % 12;
                    findPeriodEnd = 1;
                }
            }
            testInterval.push_back(daysTable[nextPeriodStartRow - 1]);
            break;
        }
        case 'Q': {
            int nextPeriodStartIndex = 3;
            int nextPeriodStartRow = testStartRow;
            for (; nextPeriodStartRow < totalDays;) {
                if (findPeriodEnd == 1) {
                    for (; nextPeriodStartRow < totalDays; nextPeriodStartRow++) {
                        if (daysTable[nextPeriodStartRow].substr(5, 2) == month[nextPeriodStartIndex]) {
                            testInterval.push_back(daysTable[nextPeriodStartRow - 1]);
                            findPeriodEnd = 0;
                            break;
                        }
                    }
                }
                else {
                    testInterval.push_back(daysTable[nextPeriodStartRow]);
                    nextPeriodStartIndex = (nextPeriodStartIndex + 3) % 12;
                    findPeriodEnd = 1;
                }
            }
            testInterval.push_back(daysTable[nextPeriodStartRow - 1]);
            break;
        }
        case 'M': {
            int nextPeriodStartIndex = 1;
            int nextPeriodStartRow = testStartRow;
            for (; nextPeriodStartRow < totalDays;) {
                if (findPeriodEnd == 1) {
                    for (; nextPeriodStartRow < totalDays; nextPeriodStartRow++) {
                        if (daysTable[nextPeriodStartRow].substr(5, 2) == month[nextPeriodStartIndex]) {
                            testInterval.push_back(daysTable[nextPeriodStartRow - 1]);
                            findPeriodEnd = 0;
                            break;
                        }
                    }
                }
                else {
                    testInterval.push_back(daysTable[nextPeriodStartRow]);
                    nextPeriodStartIndex = (nextPeriodStartIndex + 1) % 12;
                    findPeriodEnd = 1;
                }
            }
            testInterval.push_back(daysTable[nextPeriodStartRow - 1]);
            break;
        }
    }
    return testInterval;
}

void start_test() {
    vector< string > company = get_file(_output_path);  //get companies name
    vector< string > RSI_table = get_file(RSITable_path);  //get RSI table
    int companyNum = (int)company.size();
    for (int whichCompany = 0; whichCompany < companyNum; whichCompany++) {
        cout << "===========================test " + company[whichCompany] << endl;
        int totalDays = 0;
        string* daysTable = store_days_to_arr(RSITable_path + "/" + RSI_table[whichCompany], totalDays);  //記錄開始日期到結束日期
        double* priceTable = store_price_to_arr(_price_path + "/" + company[whichCompany] + ".csv", totalDays);  //記錄開始日期的股價到結束日期的股價
        double** RSITable = store_RSI_table_to_arr(RSITable_path + "/" + RSI_table[whichCompany], totalDays);  //記錄一間公司開始日期到結束日期1~256的RSI
        int windowNum = sizeof(_sliding_windows) / sizeof(_sliding_windows[0]);  //No A2A
        for (int windowUse = 0; windowUse < windowNum; windowUse++) {  //No A2A
            cout << _sliding_windows[windowUse] << endl;
            vector< string > strategy = get_file(_output_path + "/" + company[whichCompany] + "/train/" + _sliding_windows[windowUse]);  //get strategy files
            vector< string > testInterval;
            if (_sliding_windows[windowUse].length() == 3) {  //一般測試期區間
                testInterval = find_test_interval(_sliding_windows[windowUse][2], totalDays, daysTable);
            }
            else if (_sliding_windows[windowUse].length() == 4) {
                testInterval = find_test_interval(_sliding_windows[windowUse][3], totalDays, daysTable);
            }

            else {  //年對年
                testInterval = find_test_interval(_sliding_windows[windowUse][0], totalDays, daysTable);
            }
            ofstream holdPeriod;
            holdPeriod.open(_output_path + "/" + company[whichCompany] + "/testHoldPeriod/" + company[whichCompany] + "_" + _sliding_windows[windowUse] + ".csv");
            holdPeriod << "Date,Price,Hold" << endl;
            string outputPath = _output_path + "/" + company[whichCompany] + "/test/" + _sliding_windows[windowUse];
            int strategyNum = (int)strategy.size();
            for (int strategyUse = 0; strategyUse < strategyNum; strategyUse++) {
                string strategyPath = _output_path + "/" + company[whichCompany] + "/train/" + _sliding_windows[windowUse] + "/" + strategy[strategyUse];
                // cout << strategyPath << endl;
                vector< vector< string > > strategyRead = read_data(strategyPath);
                int period = stod(strategyRead[9][1]);
                int buySignal = stod(strategyRead[10][1]);
                int sellSignal = stod(strategyRead[11][1]);
                // cout << testInterval[strategyUse * 2] + "~" + testInterval[strategyUse * 2 + 1] << endl;
                cal_test_RoR(daysTable, priceTable, RSITable, testInterval[strategyUse * 2], testInterval[strategyUse * 2 + 1], period, buySignal, sellSignal, totalDays, outputPath, holdPeriod);
            }
            holdPeriod.close();
        }
        delete[] daysTable;
        delete[] priceTable;
        for (int i = 0; i < totalDays; i++) {
            delete[] RSITable[i];
        }
        delete[] RSITable;
    }
}

double cal_BH(string company, string startDate, string endDate) {
    vector< vector< string > > price = read_data("price/" + company + ".csv");
    int totalDays = (int)price.size();
    int startRow = 0;
    int endRow = 0;
    for (int i = totalDays - 1; i >= 0; i--) {
        if (price[i][0] == endDate) {
            endRow = i;
        }
        else if (price[i][0] == startDate) {
            startRow = i;
            break;
        }
    }
    int stockHold = TOTAL_CP_LV / stod(price[startRow][COL]);
    double remain = TOTAL_CP_LV - stockHold * stod(price[startRow][COL]);
    remain += stockHold * stod(price[endRow][COL]);
    return ((remain - TOTAL_CP_LV) / TOTAL_CP_LV);
}

void cal_test_IRR() {
    vector< string > company = get_file(_output_path);  //公司名稱
    ofstream IRROut;
    IRROut.open("test_IRR.csv");  //所有公司所有視窗的年化報酬率都輸出到這
    struct windowIRR {  //建立新的資料形態用來裝滑動視窗跟報酬率
        string window;
        double originIRR;
        double traditionIRR;
    } tmp;
    int companyNum = (int)company.size();
    for (int whichCompany = 0; whichCompany < companyNum; whichCompany++) {
        vector< windowIRR > IRRList;
        cout << "=====" + company[whichCompany] + "=====" << endl;
        IRROut << "=====" + company[whichCompany] + "=====,GNQTS,Tradition" << endl;
        ofstream RoROut, traditionOut;
        RoROut.open(_output_path + "/" + company[whichCompany] + "/" + company[whichCompany] + "_testRoR.csv");  //輸出一間公司所有視窗的每個區間的策略及報酬率
        traditionOut.open(_output_path + "/" + company[whichCompany] + "/" + company[whichCompany] + "_traditionRoR.csv");
        vector< string > window = get_file(_output_path + "/" + company[whichCompany] + "/test");  //視窗名稱
        int windowNum = (int)window.size();
        for (int windowIndex = 1; windowIndex < windowNum; windowIndex++) {  //No A2A，從1開始
            cout << window[windowIndex] << endl;
            double totalRate[] = {0, 0};
            RoROut << ",================" + window[windowIndex] + "================" << endl;
            traditionOut << ",================" + window[windowIndex] + "================" << endl;
            vector< string > strategy = get_file(_output_path + "/" + company[whichCompany] + "/test/" + window[windowIndex]);
            int strategyNum = (int)strategy.size();
            for (int strategys = 0; strategys < strategyNum; strategys++) {
                vector< vector< string > > originTest = read_data(_output_path + "/" + company[whichCompany] + "/test/" + window[windowIndex] + "/" + strategy[strategys]);
                vector< vector< string > > traditionTest = read_data(_output_path + "/" + company[whichCompany] + "/testTradition/" + window[windowIndex] + "/" + strategy[strategys]);
                RoROut << strategy[strategys] + "," + originTest[9][1] + "," + originTest[10][1] + "," + originTest[11][1] + "," + originTest[13][1] << endl;
                traditionOut << strategy[strategys] + "," + traditionTest[9][1] + "," + traditionTest[10][1] + "," + traditionTest[11][1] + "," + traditionTest[13][1] << endl;
                if (strategys == 0) {
                    totalRate[0] = stod(originTest[13][1]) / 100 + 1;
                    totalRate[1] = stod(traditionTest[13][1]) / 100 + 1;
                }
                else {
                    totalRate[0] = totalRate[0] * (stod(originTest[13][1]) / 100 + 1);
                    totalRate[1] = totalRate[1] * (stod(traditionTest[13][1]) / 100 + 1);
                }
            }
            tmp.window = window[windowIndex];
            tmp.originIRR = pow(totalRate[0], (double)1 / _test_length) - 1;  //計算年化報酬;
            tmp.traditionIRR = pow(totalRate[1], (double)1 / _test_length) - 1;
            totalRate[0]--;
            totalRate[1]--;
            // oneWindowRate.push_back(to_string(TotalRate));
            IRRList.push_back(tmp);
            RoROut << fixed << setprecision(10) << ",,,,,," + window[windowIndex] + "," << totalRate[0] << "," << tmp.originIRR << endl;
            traditionOut << fixed << setprecision(10) << ",,,,,," + window[windowIndex] + "," << totalRate[1] << "," << tmp.traditionIRR << endl;
        }
        RoROut.close();
        traditionOut.close();
        // windowIRR tmp;
        tmp.window = "B&H";
        vector< vector< string > > days = read_data("price/" + company[whichCompany] + ".csv");
        string testStartDate;
        for (int i = (int)days.size() - 1; i > 0; i--) {
            if (days[i][0].substr(0, 4) == to_string(stoi(_test_start_y) - 1)) {
                testStartDate = days[i + 1][0];
                break;
            }
        }
        string testEndDate = _train_end_date;
        tmp.originIRR = pow(cal_BH(company[whichCompany], testStartDate, testEndDate) + 1, (double)1 / _test_length) - 1;
        tmp.traditionIRR = tmp.originIRR;
        IRRList.push_back(tmp);

        //=======這邊是A2A的，錯誤
        // vector< string > tradition = get_file(_output_path + "/" + company[whichCompany] + "/specify");  //找出傳統投資策略的資料
        // for (int i = 0; i < tradition.size(); i++) {
        //     if (tradition[i].front() == 'R') {
        //         vector< vector< string > > specify = read_data(_output_path + "/" + company[whichCompany] + "/specify/" + tradition[i]);
        //         for (int j = (int)tradition[i].length(), k = 0; j >= 0; j--) {
        //             if (tradition[i][j] == '_') {
        //                 k++;
        //                 if (k == 3) {
        //                     tmp.window = tradition[i].substr(4, j - 1);
        //                     break;
        //                 }
        //             }
        //         }
        //         tmp.IRR = pow(stod(specify[13][1]) / 100 + 1, (double)1 / _test_length) - 1;
        //         IRRList.push_back(tmp);
        //     }
        // }
        //======
        sort(IRRList.begin(), IRRList.end(), [](const windowIRR& a, const windowIRR& b) {
            return a.traditionIRR > b.traditionIRR;
        });
        for (int i = 0; i < IRRList.size(); i++) {
            IRROut << fixed << setprecision(10) << IRRList[i].window + "," << IRRList[i].originIRR << "," << IRRList[i].traditionIRR << endl;
        }
    }
    IRROut.close();
}

void cal_train_IRR() {
    vector< string > company = get_file(_output_path);  //公司名稱
    int companyNum = (int)company.size();
    ofstream IRROut;
    IRROut.open("train_IRR.csv");
    for (int whichCompany = 0; whichCompany < companyNum; whichCompany++) {
        struct windowIRR {
            string window;
            double IRR;
        };
        vector< windowIRR > IRRList;
        cout << "=====" + company[whichCompany] + "=====" << endl;
        IRROut << "=====" + company[whichCompany] + "=====" << endl;
        ofstream RoROut;
        RoROut.open(_output_path + "/" + company[whichCompany] + "/" + company[whichCompany] + "_trainRoR.csv");  //輸出一間公司所有視窗的每個區間的策略及報酬率
        vector< string > window = get_file(_output_path + "/" + company[whichCompany] + "/train");  //視窗名稱
        int windowNum = (int)window.size();
        for (int whichWindow = 0; whichWindow < windowNum; whichWindow++) {  //No A2A
            cout << window[whichWindow] << endl;
            double TotalRate = 0;
            RoROut << ",================" + window[whichWindow] + "================" << endl;
            vector< string > strategy = get_file(_output_path + "/" + company[whichCompany] + "/train/" + window[whichWindow]);
            int strategyNum = (int)strategy.size();
            for (int strategys = 0; strategys < strategyNum; strategys++) {
                vector< vector< string > > file = read_data(_output_path + "/" + company[whichCompany] + "/train/" + window[whichWindow] + "/" + strategy[strategys]);  //視窗策略
                RoROut << strategy[strategys] + "," + file[9][1] + "," + file[10][1] + "," + file[11][1] + "," + file[13][1] << endl;
                TotalRate = TotalRate + (stod(file[13][1]) / 100);
            }
            TotalRate = TotalRate / strategyNum + 1;
            double IRR = 0;  //計算年化報酬
            if (strategyNum == 1) {
                IRR = pow(TotalRate, (double)1 / (_test_length + 1)) - 1;
            }
            else if (window[whichWindow][0] == 'Y') {
                IRR = pow(TotalRate, 1) - 1;
            }
            else if (window[whichWindow][0] == 'H') {
                IRR = pow(TotalRate, 2) - 1;
            }
            else if (window[whichWindow][0] == 'Q') {
                IRR = pow(TotalRate, 4) - 1;
            }
            else if (window[whichWindow][0] == 'M') {
                IRR = pow(TotalRate, 12) - 1;
            }
            // TotalRate--;
            windowIRR tmp;
            tmp.window = window[whichWindow];
            tmp.IRR = IRR;
            // oneWindowRate.push_back(to_string(TotalRate));
            IRRList.push_back(tmp);
            RoROut << fixed << setprecision(10) << ",,,,,," + window[whichWindow] + "," << TotalRate << "," << IRR << endl;
        }
        RoROut.close();
        windowIRR tmp;
        tmp.window = "B&H";
        tmp.IRR = pow(cal_BH(company[whichCompany], _train_start_date, _train_end_date) + 1, (double)1 / (_test_length + 1)) - 1;
        IRRList.push_back(tmp);
        sort(IRRList.begin(), IRRList.end(), [](const windowIRR& a, const windowIRR& b) {
            return a.IRR > b.IRR;
        });
        for (int i = 0; i < IRRList.size(); i++) {
            IRROut << fixed << setprecision(10) << IRRList[i].window + "," << IRRList[i].IRR << endl;
        }
    }
    IRROut.close();
}

void cal_specify_strategy(string startDate, string endDate, int period, int buySignal, int sellSignal) {
    vector< string > company = get_file(_output_path);
    vector< string > RSI_table = get_file(RSITable_path);  //get RSI table
    int companyNum = (int)company.size();
    cout << "===" << period << "-" << buySignal << "-" << sellSignal << "===" << endl;
    for (int whichCompany = 0; whichCompany < companyNum; whichCompany++) {
        cout << company[whichCompany] << endl;
        int totalDays = 0;
        string* daysTable = store_days_to_arr(RSITable_path + "/" + RSI_table[whichCompany], totalDays);  //記錄開始日期到結束日期
        double* priceTable = store_price_to_arr(_price_path + "/" + company[whichCompany] + ".csv", totalDays);  //記錄開始日期的股價到結束日期的股價
        double** RSITable = store_RSI_table_to_arr(RSITable_path + "/" + RSI_table[whichCompany], totalDays);  //記錄一間公司開始日期到結束日期1~256的RSI
        ofstream holdPeriod;
        holdPeriod.open(_output_path + "/" + company[whichCompany] + "/specify/" + "hold_" + to_string(period) + "_" + to_string(buySignal) + "_" + to_string(sellSignal) + "_" + _BH_start_day.substr(0, 4) + "_" + _BH_end_day.substr(0, 4) + ".csv");
        holdPeriod << "Date,Price,Hold" << endl;
        cal_test_RoR(daysTable, priceTable, RSITable, startDate, endDate, period, buySignal, sellSignal, totalDays, _output_path + "/" + company[whichCompany] + "/specify", holdPeriod);
        delete[] daysTable;
        delete[] priceTable;
        for (int i = 0; i < totalDays; i++) {
            delete[] RSITable[i];
        }
        delete[] RSITable;
        holdPeriod.close();
    }
}

void create_folder() {
    create_directory(_output_path);
    create_directory("IRR_split");
    vector< path > getCompany;
    copy(directory_iterator(_price_path), directory_iterator(), back_inserter(getCompany));
    sort(getCompany.begin(), getCompany.end());
    vector< string > company;
    for (int i = 0; i < getCompany.size(); i++) {
        if (getCompany[i].extension() == ".csv") {
            //cout << getCompany[i].stem() << endl;
            company.push_back(getCompany[i].stem().string());
        }
    }
    for (int i = 0; i < company.size(); i++) {
        for (int j = 0; j < sizeof(_sliding_windows) / sizeof(_sliding_windows[0]); j++) {
            create_directories(_output_path + "/" + company[i] + "/test/" + _sliding_windows[j]);
            create_directories(_output_path + "/" + company[i] + "/train/" + _sliding_windows[j]);
        }
        create_directories(_output_path + "/" + company[i] + "/testHoldPeriod");
        create_directories(_output_path + "/" + company[i] + "/trainHoldPeriod");
        create_directories(_output_path + "/" + company[i] + "/specify");
        create_directories(_output_path + "/" + company[i] + "/testBestHold");
        create_directories(_output_path + "/" + company[i] + "/trainBestHold");
        // create_directories(_output_path + "/" + company[i] + "/trainTradition");
        create_directories(_output_path + "/" + company[i] + "/testTradition");
        for (int j = 0; j < sizeof(_sliding_windows) / sizeof(_sliding_windows[0]); j++) {
            create_directories(_output_path + "/" + company[i] + "/testTradition/" + _sliding_windows[j]);
        }
        create_directories(_output_path + "/" + company[i] + "/testTraditionHoldPeriod");
        // cout << company[i] << endl;
    }
}

void remove_file() {
    vector< path > getCompany;
    copy(directory_iterator(_output_path), directory_iterator(), back_inserter(getCompany));
    sort(getCompany.begin(), getCompany.end());
    for (int i = 0; i < getCompany.size(); i++) {
        if (is_directory(getCompany[i])) {
            remove_all(getCompany[i].string() + "/trainTradition/");
            remove_all(getCompany[i].string() + "/testTradition/");
        }
    }
}

void copy_best_hold(string train_or_test, vector< string > companyBestPeriod) {
    for (int company = 0; company < companyBestPeriod.size(); company += 3) {
        cout << companyBestPeriod[company] << endl;
        string file = _output_path + "/" + companyBestPeriod[company] + "/" + train_or_test + "HoldPeriod/" + companyBestPeriod[company] + "_" + companyBestPeriod[company + 1] + ".csv";
        string to = _output_path + "/" + companyBestPeriod[company] + "/" + train_or_test + "bestHold/";
        filesystem::copy(file, to, copy_options::overwrite_existing);
    }
}

void find_best_hold(string train_or_test) {
    vector< vector< string > > all_IRR = read_data(train_or_test + "_IRR.csv");
    vector< string > companyBestPeriod;
    for (int i = 0; i < all_IRR.size(); i++) {
        if (all_IRR[i][0].front() == '=') {
            string toPush = all_IRR[i][0];
            toPush.erase(remove(toPush.begin(), toPush.end(), '='), toPush.end());
            companyBestPeriod.push_back(toPush);
            for (int j = 1; j < 10; j++) {
                if (all_IRR[i + j][0] != "B&H" && isalpha(all_IRR[i + j][0].front())) {
                    companyBestPeriod.push_back(all_IRR[i + j][0]);
                    companyBestPeriod.push_back(all_IRR[i + j][1]);
                    break;
                }
            }
        }
    }
    copy_best_hold(train_or_test, companyBestPeriod);
}

double cal_train_tradition_RoR(string* daysTable, double* priceTable, double** RSITable, string startDate, string endDate, int period, int buySignal, int sellSignal, int totalDays) {
    int startingRow = 0;
    for (int i = 0; i < totalDays; i++) {
        if (daysTable[i] == startDate) {
            startingRow = i;
            break;
        }
    }
    int endingRow = 0;
    for (int i = startingRow; i < totalDays; i++) {
        if (daysTable[i] == endDate) {
            endingRow = i;
            break;
        }
    }
    // cout << daysTable[startingRow] << endl;
    // cout << daysTable[endingRow] << endl;
    int stockHold = 0;
    double remain = TOTAL_CP_LV;
    // int flag = 0;  //記錄手上有錢還是有股票
    vector< string > tradeRecord;
    for (int i = startingRow; i <= endingRow; i++) {
        if (period != 0 && RSITable[i][period] <= buySignal && stockHold == 0 && i < endingRow) {  //買入訊號出現且無持股
            stockHold = remain / priceTable[i];
            remain -= (double)stockHold * priceTable[i];
        }
        else if (period != 0 && ((RSITable[i][period] >= sellSignal && stockHold != 0) || (stockHold != 0 && i == endingRow))) {  //賣出訊號出現且有持股
            remain += (double)stockHold * priceTable[i];
            stockHold = 0;
        }
    }
    return (remain - TOTAL_CP_LV) / TOTAL_CP_LV;
}

void test_tradition(string* daysTable, double* priceTable, double** RSITable, int totalDays, string company, vector< vector< double > > strategy, string windowUse) {
    cout << "===========================test tradition " + windowUse << endl;
    vector< string > testInterval;
    if (windowUse.length() == 3) {  //一般測試期區間
        testInterval = find_test_interval(windowUse[2], totalDays, daysTable);
    }
    else {  //年對年
        testInterval = find_test_interval(windowUse[0], totalDays, daysTable);
    }
    // for (int i = 0; i < testInterval.size(); i += 2) {
    //     cout << testInterval[i] << "~" << testInterval[i + 1] << endl;
    // }
    ofstream holdPeriod;
    holdPeriod.open(_output_path + "/" + company + "/testTraditionHoldPeriod/" + company + "_" + windowUse + ".csv");
    holdPeriod << "Date,Price,Hold" << endl;
    string RoROutputPath = _output_path + "/" + company + "/testTradition/" + windowUse;
    int strategyNum = (int)strategy.size();
    for (int strategyUse = 0; strategyUse < strategyNum; strategyUse++) {
        // cout << strategyPath << endl;
        int period = (int)strategy[strategyUse][0];
        int buySignal = (int)strategy[strategyUse][1];
        int sellSignal = (int)strategy[strategyUse][2];
        // cout << testInterval[strategyUse * 2] + "~" + testInterval[strategyUse * 2 + 1] << endl;
        cal_test_RoR(daysTable, priceTable, RSITable, testInterval[strategyUse * 2], testInterval[strategyUse * 2 + 1], period, buySignal, sellSignal, totalDays, RoROutputPath, holdPeriod);
    }
    holdPeriod.close();
}

void train_tradition() {
    int traditionStrategy[6][3] = {{5, 20, 80}, {5, 30, 70}, {6, 20, 80}, {6, 30, 70}, {14, 20, 80}, {14, 30, 70}};
    vector< string > allRSITalble = get_file(RSITable_path);  //get RSI table
    vector< string > allStockPrice = get_file(_price_path);  //get stock price
    vector< string > allCompany = get_file(_output_path);
    int companyNum = (int)allCompany.size();
    for (int companyIndex = 0; companyIndex < companyNum; companyIndex++) {
        cout << "===========================train_tradition " << allStockPrice[companyIndex] << endl;
        int totalDays = 0;
        string* daysTable = store_days_to_arr(RSITable_path + "/" + allRSITalble[companyIndex], totalDays);  //記錄開始日期到結束日期
        double* priceTable = store_price_to_arr(_price_path + "/" + allStockPrice[companyIndex], totalDays);  //記錄開始日期的股價到結束日期的股價
        double** RSITable = store_RSI_table_to_arr(RSITable_path + "/" + allRSITalble[companyIndex], totalDays);  //記錄一間公司開始日期到結束日期1~256的RSI
        int windowNum = sizeof(_sliding_windows) / sizeof(_sliding_windows[0]);
        for (int windowIndex = 0; windowIndex < windowNum; windowIndex++) {  //不要A2A
            vector< int > intervalTable;  //記錄視窗區間
            find_interval(totalDays, windowIndex, daysTable, intervalTable);
            int intervalNum = (int)intervalTable.size();
            vector< vector< double > > recordTrainStrategy(intervalNum / 2, vector< double >(4));  //記錄訓練結果
            for (int intervalIndex = 0; intervalIndex < intervalNum; intervalIndex += 2) {
                cout << "===" + daysTable[intervalTable[intervalIndex]] + "~" + daysTable[intervalTable[intervalIndex + 1]] + "===" << endl;
                int traditionNum = sizeof(traditionStrategy) / sizeof(traditionStrategy[0]);
                for (int whichTradition = 0; whichTradition < traditionNum; whichTradition++) {
                    double tmpRoR = cal_train_tradition_RoR(daysTable, priceTable, RSITable, daysTable[intervalTable[intervalIndex]], daysTable[intervalTable[intervalIndex + 1]], traditionStrategy[whichTradition][0], traditionStrategy[whichTradition][1], traditionStrategy[whichTradition][2], totalDays);
                    // cout << traditionStrategy[whichTradition][0] << "_" << traditionStrategy[whichTradition][1] << "_" << traditionStrategy[whichTradition][2] << ":" << tmpRoR << endl;
                    if (tmpRoR >= recordTrainStrategy[intervalIndex / 2][3]) {
                        for (int i = 0; i < 3; i++) {
                            recordTrainStrategy[intervalIndex / 2][i] = (double)traditionStrategy[whichTradition][i];
                        }
                        recordTrainStrategy[intervalIndex / 2][3] = tmpRoR;
                    }
                }
                for (int i = 0; i < 4; i++) {
                    cout << recordTrainStrategy[intervalIndex / 2][i] << "_";
                }
                cout << endl;
                // cout << recordTrainStrategy[intervalIndex / 2][3] << endl;
            }
            // for (int i = 0; i < recordTrainStrategy.size(); i++) {
            //     for (int j = 0; j < recordTrainStrategy[i].size(); j++) {
            //         cout << recordTrainStrategy[i][j] << endl;
            //     }
            // }
            test_tradition(daysTable, priceTable, RSITable, totalDays, allCompany[companyIndex], recordTrainStrategy, _sliding_windows[windowIndex]);
            vector< int >().swap(intervalTable);
        }
        delete[] daysTable;
        delete[] priceTable;
        for (int i = 0; i < totalDays; i++) {
            delete[] RSITable[i];
        }
        delete[] RSITable;
    }
}

void make_RSI_table() {
    string startY = "2009";
    string endY = "2021";
    vector< path > companyPath;
    copy(directory_iterator("RSI"), directory_iterator(), back_inserter(companyPath));
    sort(companyPath.begin(), companyPath.end());
    vector< path > companyName;
    copy(directory_iterator("price"), directory_iterator(), back_inserter(companyName));
    sort(companyName.begin(), companyName.end());
    for (int companyIndex = 0; companyIndex < companyPath.size(); companyIndex++) {
        if (stoi(startY) > 2009 || companyName[companyIndex].stem().string() != "V") {  //V要2009年以後，不然會錯誤
            cout << companyName[companyIndex].stem() << endl;
            vector< path > RSIFilePath;
            copy(directory_iterator(companyPath[companyIndex]), directory_iterator(), back_inserter(RSIFilePath));
            sort(RSIFilePath.begin(), RSIFilePath.end());
            vector< vector< string > > RSI1 = read_data(RSIFilePath[0].string());
            int totalDays = (int)RSI1.size();
            int startRow = 0;
            int endRow = 0;
            for (int i = 0, j = 0; i < totalDays; i++) {
                if (j == 0 && RSI1[i][0].substr(0, 4) == startY) {
                    startRow = i;
                    j++;
                }
                else if (j == 1 && RSI1[i][0].substr(0, 4) == endY) {
                    endRow = i - 1;
                    j++;
                }
            }
            int trainDays = endRow - startRow + 1;
            // for (int i = 0; i < RSI1.size(); i++) {
            //     for (int j = 0; j < RSI1[i].size(); j++) {
            //         cout << RSI1[i][j] + "\t";
            //     }
            //     cout << endl;
            // }
            // cout << company[companyIndex].stem() << endl;
            // cout << RSI1[startRow][0] << endl;
            // cout << RSI1[endRow][0] << endl;

            string** RSITable = new string*[trainDays];
            for (int i = 0; i < trainDays; i++) {
                RSITable[i] = new string[257];
            }
            for (int i = 0, j = startRow; i < trainDays; i++, j++) {
                RSITable[i][0] = RSI1[j][0];
                RSITable[i][1] = RSI1[j][1];
            }
            for (int RSIFileIndex = 1, RSINow = 2; RSIFileIndex < RSIFilePath.size(); RSIFileIndex++, RSINow++) {
                vector< vector< string > > RSI = read_data(RSIFilePath[RSIFileIndex].string());
                for (int i = 0, j = 0; i < totalDays; i++) {
                    if (j == 0 && RSI[i][0].substr(0, 4) == startY) {
                        startRow = i;
                        j++;
                    }
                    else if (j == 1 && RSI[i][0].substr(0, 4) == endY) {
                        endRow = i - 1;
                        j++;
                    }
                }
                for (int i = 0, j = startRow; i < trainDays; i++, j++) {
                    RSITable[i][RSINow] = RSI[j][1];
                }
            }
            ofstream out;
            out.open("RSI_table/" + companyName[companyIndex].stem().string() + "_RSI_table.csv");
            for (int i = 0; i < trainDays; i++) {
                for (int j = 0; j < 257; j++) {
                    out << RSITable[i][j] + ",";
                }
                out << endl;
            }
            out.close();
            for (int i = 0; i < trainDays; i++) {  //V從2008才開始，在RSI2XX的時候會超過2009，delete會出錯
                delete[] RSITable[i];
            }
            delete[] RSITable;
        }
    }
}

int main(void) {
    create_folder();
    switch (mode) {
        case 0:
            start_train();
            break;
        case 1:
            cal_train_IRR();
            break;
        case 2:
            start_test();
            break;
        case 3:
            train_tradition();
            break;
        case 4:
            cal_test_IRR();
            break;
        case 5:
            cal_specify_strategy(_BH_start_day, _BH_end_day, 5, 20, 80);
            cal_specify_strategy(_BH_start_day, _BH_end_day, 5, 30, 70);
            cal_specify_strategy(_BH_start_day, _BH_end_day, 6, 20, 80);
            cal_specify_strategy(_BH_start_day, _BH_end_day, 6, 30, 70);
            cal_specify_strategy(_BH_start_day, _BH_end_day, 14, 20, 80);
            cal_specify_strategy(_BH_start_day, _BH_end_day, 14, 30, 70);
            break;
        case 6:
            find_best_hold("test");
            break;
        case 7:
            cal_specify_strategy(_BH_start_day, _BH_end_day, _period, _buySignal, _sellSignal);
            break;
        case 8:
            cout << fixed << setprecision(10) << cal_BH(_BH_company, _BH_start_day, _BH_end_day) * 100 << "%" << endl;
            break;
        case 9:
            remove_file();
            break;
        case 10:
            make_RSI_table();
            break;
        default:
            cout << "Wrong mode" << endl;
            break;
    }
}
