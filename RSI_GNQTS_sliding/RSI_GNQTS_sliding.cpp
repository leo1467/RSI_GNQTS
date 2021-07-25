//sliding windows
//finished on 2021/05/13
//
//
//
#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// #include "dirent.h"

using namespace std;

#define PARTICAL_AMOUNT 10
#define START 2  //股價csv裡的開始欄位
#define COL 4  //股價在第幾COLumn
#define TOTAL_CP_LV 10000000.0

#define MODE 1  //0:train, 1:test

double delta = 0.003;
int exp_times = 50;
int generation = 1000;

string starting_date = "2010-01-04";
string ending_date = "2020-12-31";
string test_start_y = to_string(stoi(starting_date.substr(0, 4)) + 1);
string test_start_m = starting_date.substr(5, 2);
string sliding_windows[] = {"A2A", "Y2Y", "Y2H", "Y2Q", "Y2M", "H#", "H2H", "H2Q", "H2M", "Q#", "Q2Q", "Q2M", "M#", "M2M"};

string RSI_path = "/Users/neo/Desktop/VScode/new training/RSI/all_RSI_table";
string price_path = "/Users/neo/Desktop/VScode/new training/RSI/all_price";
string output_path = "/Users/neo/Desktop/VScode/new training/RSI/all_sw";
// string RSI_path = "D:/stock_info/all_RSI_table";
// string price_path = "D:/stock_info/all_price";
// string output_path = "D:/stock_info/all_sw";

string* days_table;  //記錄開始日期到結束日期
double** big_RSI_table;  //記錄一間公司開始日期到結束日期1~256的RSI
double* stock_table;  //記錄開始日期的股價到結束日期的股價
vector< int > interval_table;  //記錄滑動區間

struct prob {
    double period[8];
    double buying_signal[7];  //oversold
    double selling_signal[7];  //overbought
} prob_matrix;

struct partical {
    int period_bi[8];
    int buying_signal_bi[7];  //oversold
    int selling_signal_bi[7];  //overbought
    int period_dec;
    int buying_signal_dec;  //oversold
    int selling_signal_dec;  //overbought
    double RoR;
    double final_cp_lv;
    int trading_times;
} partical[PARTICAL_AMOUNT], the_best, Gbest, Gworst, Lbest, Lworst;

void create_folder(string company, string slide_folder) {
    string s;
    if (slide_folder == "company") {
        for (int i = 0; i < 4; i++) {
            company.pop_back();
        }
        s = output_path + "/" + company;
    }
    else if (slide_folder == "train" || slide_folder == "test") {
        s = output_path + "/" + company + "/" + slide_folder;
    }
    else {
        s = output_path + "/" + company + "/train/" + slide_folder;
        struct stat info;
        if (stat(s.c_str(), &info) != 0) {
            cout << "cannot access " << s << endl;
            if (mkdir(s.c_str(), 0777) == -1) {
                cerr << "Error : " << strerror(errno) << endl;
            }
            else {
                cout << "Directory created" << endl;
            }
        }
        s = output_path + "/" + company + "/test/" + slide_folder;
    }
    struct stat info;
    if (stat(s.c_str(), &info) != 0) {
        cout << "cannot access " << s << endl;
        if (mkdir(s.c_str(), 0777) == -1) {
            cerr << "Error : " << strerror(errno) << endl;
        }
        else {
            cout << "Directory created" << endl;
        }
    }
}

vector< vector< string > > readData(string filename) {
    // cout << filename << endl;
    ifstream infile(filename, ios::in);
    string line;
    vector< vector< string > > data;
    if (!infile) {
        cout << "open file failed" << endl;
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

vector< string > get_file(string RSI_table_path) {
    DIR* dir;
    struct dirent* ptr;
    vector< string > file_name;
    dir = opendir(RSI_table_path.c_str());
    while ((ptr = readdir(dir)) != NULL) {
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0 || strcmp(ptr->d_name, ".DS_Store") == 0) {
            continue;
        }
        file_name.push_back(ptr->d_name);
        // cout << ptr->d_name << endl;
    }
    closedir(dir);
    sort(file_name.begin(), file_name.end());
    return file_name;
}

vector< string > get_folder() {
    DIR* dir;
    struct dirent* ptr;
    vector< string > folder_name;
    dir = opendir(output_path.c_str());
    while ((ptr = readdir(dir)) != NULL) {
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0 || strcmp(ptr->d_name, ".DS_Store") == 0) {
            continue;
        }
        folder_name.push_back(ptr->d_name);
    }
    closedir(dir);
    sort(folder_name.begin(), folder_name.end());
    return folder_name;
}

vector< string > get_stock_file() {
    DIR* dir;
    struct dirent* ptr;
    vector< string > file_name;
    dir = opendir(price_path.c_str());
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

void local_ini() {
    for (int i = 0; i < PARTICAL_AMOUNT; i++) {
        for (int j = 0; j < 8; j++) {
            partical[i].period_bi[j] = 0;
        }
        for (int j = 0; j < 7; j++) {
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

    for (int i = 0; i < 8; i++) {
        Lbest.period_bi[i] = 0;
        Lworst.period_bi[i] = 0;
    }
    for (int i = 0; i < 7; i++) {
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

void global_ini() {
    // for (int i = 0; i < PARTICAL_AMOUNT; i++) {
    //     for (int j = 0; j < 8; j++) {
    //         partical[i].period_bi[j] = 0;
    //     }
    //     for (int j = 0; j < 7; j++) {
    //         partical[i].buying_signal_bi[j] = 0;
    //         partical[i].selling_signal_bi[j] = 0;
    //     }
    //     partical[i].period_dec = 0;
    //     partical[i].buying_signal_dec = 0;
    //     partical[i].selling_signal_dec = 0;
    //     partical[i].RoR = 0;
    //     partical[i].final_cp_lv = 0;
    //     partical[i].trading_times = 0;
    // }
    for (int i = 0; i < 8; i++) {
        Gbest.period_bi[i] = 0;
        Gworst.period_bi[i] = 0;
    }
    for (int i = 0; i < 7; i++) {
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

void the_best_ini() {
    for (int i = 0; i < 8; i++) {
        the_best.period_bi[i] = 0;
    }
    for (int i = 0; i < 7; i++) {
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

void beta_matrix_ini() {
    for (int i = 0; i < 8; i++) {
        prob_matrix.period[i] = 0.5;
    }
    for (int i = 0; i < 7; i++) {
        prob_matrix.buying_signal[i] = 0.5;
        prob_matrix.selling_signal[i] = 0.5;
    }
}

void measure(/*ofstream& debug*/) {
    double r;
    for (int i = 0; i < PARTICAL_AMOUNT; i++) {
        // debug << "r" << i << ",";
        for (int j = 0; j < 8; j++) {
            r = rand();
            r = r / (double)RAND_MAX;
            if (r < prob_matrix.period[j]) {
                partical[i].period_bi[j] = 1;
            }
            else {
                partical[i].period_bi[j] = 0;
            }
            // debug << r << ",";
        }
        for (int j = 0; j < 7; j++) {
            r = rand();
            r = r / (double)RAND_MAX;
            if (r < prob_matrix.buying_signal[j]) {
                partical[i].buying_signal_bi[j] = 1;
            }
            else {
                partical[i].buying_signal_bi[j] = 0;
            }
            // debug << r << ",";
        }
        for (int j = 0; j < 7; j++) {
            r = rand();
            r = r / (double)RAND_MAX;
            if (r < prob_matrix.selling_signal[j]) {
                partical[i].selling_signal_bi[j] = 1;
            }
            else {
                partical[i].selling_signal_bi[j] = 0;
            }
            // debug << r << ",";
        }
        // debug << endl;
    }
}

void bi_to_dec() {
    for (int i = 0; i < PARTICAL_AMOUNT; i++) {
        partical[i].period_dec = 0;
        partical[i].buying_signal_dec = 0;
        partical[i].selling_signal_dec = 0;
        for (int j = 0, k = 7; j < 8; j++, k--) {
            partical[i].period_dec += pow(2, k) * partical[i].period_bi[j];
        }
        for (int j = 0, k = 6; j < 7; j++, k--) {
            partical[i].buying_signal_dec += pow(2, k) * partical[i].buying_signal_bi[j];
            partical[i].selling_signal_dec += pow(2, k) * partical[i].selling_signal_bi[j];
        }
        partical[i].period_dec++;
    }
}

void local_update(int partical_num) {
    if (partical[partical_num].RoR > Lbest.RoR) {
        for (int i = 0; i < 8; i++) {
            Lbest.period_bi[i] = partical[partical_num].period_bi[i];
        }
        for (int i = 0; i < 7; i++) {
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
        for (int i = 0; i < 8; i++) {
            Lworst.period_bi[i] = partical[partical_num].period_bi[i];
        }
        for (int i = 0; i < 7; i++) {
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

void global_update(/*ofstream& debug*/) {
    // debug << "beta,";
    // for (int i = 0; i < 8; i++) {
    //     debug << prob_matrix.period[i] << ",";
    // }
    // for (int i = 0; i < 7; i++) {
    //     debug << prob_matrix.buying_signal[i] << ",";
    // }
    // for (int i = 0; i < 7; i++) {
    //     debug << prob_matrix.selling_signal[i] << ",";
    // }
    // debug << endl;

    if (Lbest.RoR > Gbest.RoR) {
        for (int i = 0; i < 8; i++) {
            Gbest.period_bi[i] = Lbest.period_bi[i];
        }
        for (int i = 0; i < 7; i++) {
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

    // debug << "global best,";
    // for (int i = 0; i < 8; i++) {
    //     debug << Gbest.period_bi[i] << ",";
    // }
    // for (int i = 0; i < 7; i++) {
    //     debug << Gbest.buying_signal_bi[i] << ",";
    // }
    // for (int i = 0; i < 7; i++) {
    //     debug << Gbest.selling_signal_bi[i] << ",";
    // }
    // debug << Gbest.period_dec << "," << Gbest.buying_signal_dec << "," << Gbest.selling_signal_dec << "," << Gbest.RoR << endl;

    // debug << "local worst,";
    // for (int i = 0; i < 8; i++) {
    //     debug << Lworst.period_bi[i] << ",";
    // }
    // for (int i = 0; i < 7; i++) {
    //     debug << Lworst.buying_signal_bi[i] << ",";
    // }
    // for (int i = 0; i < 7; i++) {
    //     debug << Lworst.selling_signal_bi[i] << ",";
    // }
    // debug << Lworst.period_dec << "," << Lworst.buying_signal_dec << "," << Lworst.selling_signal_dec << "," << Lworst.RoR << endl;
    //===============================================================================
    for (int i = 0; i < 8; i++) {
        if (Gbest.period_bi[i] == 1 && Lworst.period_bi[i] == 0 && prob_matrix.period[i] < 0.5) {
            prob_matrix.period[i] = 1.0 - prob_matrix.period[i];
        }
        if (Gbest.period_bi[i] == 0 && Lworst.period_bi[i] == 1 && prob_matrix.period[i] > 0.5) {
            prob_matrix.period[i] = 1.0 - prob_matrix.period[i];
        }
        if (Gbest.period_bi[i] == 1) {
            prob_matrix.period[i] += delta;
        }
        if (Lworst.period_bi[i] == 1) {
            prob_matrix.period[i] -= delta;
        }
    }
    for (int i = 0; i < 7; i++) {
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
            prob_matrix.buying_signal[i] += delta;
        }
        if (Gbest.selling_signal_bi[i] == 1) {
            prob_matrix.selling_signal[i] += delta;
        }
        if (Lworst.buying_signal_bi[i] == 1) {
            prob_matrix.buying_signal[i] -= delta;
        }
        if (Lworst.selling_signal_bi[i] == 1) {
            prob_matrix.selling_signal[i] -= delta;
        }
    }
    //===============================================================================
    // debug << "beta,";
    // for (int i = 0; i < 8; i++) {
    //     debug << prob_matrix.period[i] << ",";
    // }
    // for (int i = 0; i < 7; i++) {
    //     debug << prob_matrix.buying_signal[i] << ",";
    // }
    // for (int i = 0; i < 7; i++) {
    //     debug << prob_matrix.selling_signal[i] << ",";
    // }
    // debug << endl;
}

void the_best_update() {
    if (the_best.RoR < Gbest.RoR) {
        for (int i = 0; i < 8; i++) {
            the_best.period_bi[i] = Gbest.period_bi[i];
        }
        for (int i = 0; i < 7; i++) {
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

int find_train_end(int table_size, string mon, int& train_end_row, int to_jump) {
    int skip = 0;
    for (int i = table_size - 1; i >= 0; i--) {
        if (days_table[i].substr(5, 2) != mon) {
            skip++;
            mon = days_table[i].substr(5, 2);
            if (skip == to_jump) {
                train_end_row = i;
                break;
            }
            i -= 15;
        }
    }
    return skip;
}

int find_train_start(int test_s_row, string mon, int& train_start_row, string& start, string& end, int to_jump) {
    int skip = 0;
    if (to_jump == 13) {  //給Y2Y用
        train_start_row = 0;
        start = days_table[0].substr(5, 2);
        end = days_table[test_s_row].substr(5, 2);
    }
    else {
        for (int i = test_s_row; i >= -1; i--) {
            if (days_table[i].substr(5, 2) != mon) {
                skip++;
                mon = days_table[i].substr(5, 2);
                if (skip == to_jump) {
                    train_start_row = i + 1;
                    start = days_table[i + 1].substr(5, 2);
                    end = days_table[test_s_row].substr(5, 2);
                    break;
                }
                i -= 15;
            }
        }
    }
    return to_jump;
}

void sliding_start_end(int table_size, string SW, int sliding_type_int) {
    string M[] = {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12"};
    int test_s_row = 0;  //記錄測試期開始row
    for (int i = 230; i < 300; i++) {  //找出測試期開始的row
        if (days_table[i].substr(0, 4) == test_start_y) {
            test_s_row = i;
            break;
        }
    }
    int train_start_row = 0;  //記錄訓練期開始的row
    int train_end_row = 0;  //記錄訓練期結束的row
    if (sliding_type_int == 2) {  //判斷year-on-year
        train_start_row = 0;
        string train_end_y = to_string(stoi(ending_date.substr(0, 4)) - 1);
        for (int i = table_size - 230; i > 0; i--) {
            if (days_table[i].substr(0, 4) == train_end_y) {
                train_end_row = i;
                break;
            }
        }
        switch (SW[0]) {  //做H*
            case 'H': {
                interval_table.push_back(train_start_row);
                string end_H = M[6];
                for (int i = 97, j = 6; i < train_end_row; i++) {
                    if (days_table[i].substr(5, 2) == end_H) {
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
                    if (days_table[i].substr(5, 2) == end_Q) {
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
                    if (days_table[i].substr(5, 2) == end_H) {
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
        string mon = test_start_m;  //記錄目前iterate到什麼月份
        string start;  //記錄sliding window開始月份
        string end;  //記錄sliding window結束月份
        int s_jump_e = 0;  //從period頭要找period尾
        int e_jump_s = 0;  //從period尾要找下一個period頭
        int skip = 0;  //M[]要跳幾個月份
        switch (SW[0]) {  //看是什麼開頭，找出第一個訓練開始的row以及開始月份及結束月份
            case 'Y': {
                s_jump_e = find_train_start(test_s_row, mon, train_start_row, start, end, 13) - 1;
                break;
            }
            case 'H': {
                s_jump_e = find_train_start(test_s_row, mon, train_start_row, start, end, 7) - 1;
                break;
            }
            case 'Q': {
                s_jump_e = find_train_start(test_s_row, mon, train_start_row, start, end, 4) - 1;
                break;
            }
            case 'M': {
                s_jump_e = find_train_start(test_s_row, mon, train_start_row, start, end, 2) - 1;
                break;
            }
        }
        switch (SW[2]) {  //看是什麼結尾，找出最後一個訓練開始的row
            case 'Y': {
                skip = find_train_end(table_size, mon, train_end_row, 13) - 1;
                break;
            }
            case 'H': {
                skip = find_train_end(table_size, mon, train_end_row, 7) - 1;
                break;
            }
            case 'Q': {
                skip = find_train_end(table_size, mon, train_end_row, 4) - 1;
                break;
            }
            case 'M': {
                skip = find_train_end(table_size, mon, train_end_row, 2) - 1;
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
        if (SW[0] == SW[2]) {  //判斷從period尾要找下一個period頭需要跳多少
            e_jump_s = 1;
        }
        else if (SW[2] == 'M') {
            e_jump_s = s_jump_e - 1;
        }
        else if (SW[2] == 'Q') {
            e_jump_s = s_jump_e - 3;
        }
        else {
            e_jump_s = 6;
        }
        interval_table.push_back(train_start_row);
        for (int i = train_start_row + s_jump_e * 18, k = 1; i < train_end_row; i++) {
            if (k % 2 == 1 && days_table[i].substr(5, 2) == end) {
                interval_table.push_back(i - 1);
                start = M[(train_s_M += skip) % 12];
                end = M[(train_e_M += skip) % 12];
                i -= e_jump_s * 22;
                if (i < 0) {
                    i = 0;
                }
                k++;
            }
            else if (k % 2 == 0 && days_table[i].substr(5, 2) == start) {
                interval_table.push_back(i);
                i += s_jump_e * 18;
                k++;
            }
        }
        interval_table.push_back(train_end_row);
    }
}

void find_interval(int table_size, int slide) {  //將A2A跟其他分開
    cout << "looking for interval " + sliding_windows[slide] << endl;
    if (sliding_windows[slide] == "A2A") {  //直接給A2A的起始與結束row
        interval_table.push_back(0);
        interval_table.push_back(table_size - 1);
    }
    else {  //開始找普通滑動視窗的起始與結束
        sliding_start_end(table_size, sliding_windows[slide], (int)sliding_windows[slide].size());
    }
    for (int i = 0; i < interval_table.size(); i += 2) {
        cout << days_table[interval_table[i]] + "~" + days_table[interval_table[i + 1]] << endl;
    }
}

int store_RSI_and_price(string RSI_table_path, string stock_file_path, int slide) {
    vector< vector< string > > RSI_table_in = readData(RSI_table_path);
    int table_size = (int)RSI_table_in.size();
    days_table = new string[table_size];
    for (int i = 0; i < table_size; i++) {
        days_table[i] = RSI_table_in[i][0];
    }
    big_RSI_table = new double*[table_size];
    for (int i = 0; i < table_size; i++) {
        big_RSI_table[i] = new double[257];
        for (int j = 1; j < 257; j++) {
            big_RSI_table[i][j] = stod(RSI_table_in[i][j]);
        }
    }
    vector< vector< string > > stock_price_in = readData(stock_file_path);
    int price_size = (int)stock_price_in.size();
    int startingRow = 0;
    int ending_row = 0;
    for (int i = 0; i < price_size; i++) {
        if (stock_price_in[i][0] == starting_date) {
            startingRow = i;
            for (; i < price_size; i++) {
                if (stock_price_in[i][0] == ending_date) {
                    ending_row = i;
                    break;
                }
            }
            break;
        }
    }
    stock_table = new double[table_size];
    for (int i = 0, j = startingRow; j <= ending_row; i++, j++) {
        stock_table[i] = stod(stock_price_in[j][COL]);
    }
    if (MODE == 0) {
        find_interval(table_size, slide);  //找出滑動間格
    }
    return table_size;
}

void cal_RoR(int interval_index /*, ofstream& debug*/, int& earlestGen, int gen) {
    int stock_held = 0;
    double remain = TOTAL_CP_LV;
    int flag = 0;  //記錄是否有交易過
    for (int partical_num = 0; partical_num < PARTICAL_AMOUNT; partical_num++) {
        stock_held = 0;
        remain = TOTAL_CP_LV;
        flag = 0;
        // ===============================================================開始計算報酬率
        for (int i = interval_table[interval_index]; i <= interval_table[interval_index + 1]; i++) {
            if (big_RSI_table[i][partical[partical_num].period_dec] <= partical[partical_num].buying_signal_dec && stock_held == 0) {  //買入訊號出現且無持股
                stock_held = remain / stock_table[i];
                remain = remain - stock_table[i] * stock_held;
                flag = 1;
                partical[partical_num].trading_times++;
                if (i == interval_table[interval_index + 1]) {
                    partical[partical_num].trading_times--;
                }
            }
            else if (big_RSI_table[i][partical[partical_num].period_dec] >= partical[partical_num].selling_signal_dec && stock_held != 0) {  //賣出訊號出現且有持股
                remain += (double)stock_held * stock_table[i];
                stock_held = 0;
            }
        }
        if (flag == 0) {  //沒有交易過
            partical[partical_num].RoR = 0;
            partical[partical_num].final_cp_lv = 0;
        }
        else if (stock_held != 0) {  //有交易過且有持股
            remain += stock_held * stock_table[interval_table[interval_index + 1]];
            partical[partical_num].RoR = ((remain - TOTAL_CP_LV) / TOTAL_CP_LV) * 100;
            partical[partical_num].final_cp_lv = remain;
        }
        else {  //有交易過但沒有持股
            partical[partical_num].RoR = ((remain - TOTAL_CP_LV) / TOTAL_CP_LV) * 100;
            partical[partical_num].final_cp_lv = remain;
        }
        // ===============================================================報酬率計算結束
        local_update(partical_num);
        // debug << "p" << partical_num << ",";
        // for (int i = 0; i < 8; i++) {
        //     debug << partical[partical_num].period_bi[i] << ",";
        // }
        // for (int i = 0; i < 7; i++) {
        //     debug << partical[partical_num].buying_signal_bi[i] << ",";
        // }
        // for (int i = 0; i < 7; i++) {
        //     debug << partical[partical_num].selling_signal_bi[i] << ",";
        // }
        // debug << partical[partical_num].period_dec << "," << partical[partical_num].buying_signal_dec << "," << partical[partical_num].selling_signal_dec << "," << partical[partical_num].RoR << endl;
    }
    if (Gbest.RoR < Lbest.RoR) {
        earlestGen = gen;
    }
    if (Lbest.RoR != 0) {
        global_update(/*debug*/);
    }
}

void cal(int interval_index /*, ofstream& debug*/, int& earlestGen) {
    global_ini();
    beta_matrix_ini();
    for (int gen = 0; gen < generation; gen++) {
        // debug << "gen: " << gen << endl;
        local_ini();
        measure(/*debug*/);
        bi_to_dec();
        cal_RoR(interval_index /*, debug*/, earlestGen, gen);
    }
    // cout << the_best.RoR << "%" << endl;
}

void output(int interval_index, int slide, string company, int earlestExp, int earlestGen) {
    ofstream data;
    data.open(output_path + "/" + company + "/train/" + sliding_windows[slide] + "/" +
              days_table[interval_table[interval_index]] + "_" + days_table[interval_table[interval_index + 1]] + ".csv");
    data << "Generation," << generation << endl;
    data << "Partical amount," << PARTICAL_AMOUNT << endl;
    data << "Delta," << delta << endl;
    data << "EXP times," << exp_times << endl;
    data << endl;
    data << fixed << setprecision(0) << "Initial capital," << TOTAL_CP_LV << endl;
    if (the_best.RoR == 0) {
        data << fixed << setprecision(2) << "Final capital," << TOTAL_CP_LV << endl;
        data << fixed << setprecision(2) << "Final return,0" << endl;
    }
    else {
        data << fixed << setprecision(2) << "Final capital," << the_best.final_cp_lv << endl;
        data << fixed << setprecision(2) << "Final return," << the_best.final_cp_lv - TOTAL_CP_LV << endl;
    }
    data << endl;
    data << "Period," << the_best.period_dec << endl;
    data << "Buying signal," << the_best.buying_signal_dec << endl;
    data << "Selling signal," << the_best.selling_signal_dec << endl;
    data << "Trading times," << the_best.trading_times << endl;
    data << fixed << setprecision(2) << "Rate of return," << the_best.RoR << "%" << endl;
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
            if (big_RSI_table[i][the_best.period_dec] <= the_best.buying_signal_dec && stock_held == 0) {  //買入訊號出現且無持股
                stock_held = remain / stock_table[i];
                remain = remain - stock_table[i] * stock_held;
                if (i != interval_table[interval_index + 1]) {
                    data << "Buy," + days_table[i] + "," << stock_table[i] << "," << big_RSI_table[i][the_best.period_dec] << "," << stock_held;
                    data << "," << remain << "," << remain + stock_table[i] * stock_held << endl;
                }
                if (i == interval_table[interval_index + 1]) {
                    flag = 1;
                }
            }
            else if (big_RSI_table[i][the_best.period_dec] >= the_best.selling_signal_dec && stock_held != 0) {  //賣出訊號出現且有持股
                remain += (double)stock_held * stock_table[i];
                stock_held = 0;
                data << "Sell," + days_table[i] + "," << stock_table[i] << "," << big_RSI_table[i][the_best.period_dec] << "," << stock_held;
                data << "," << remain << "," << remain + stock_table[i] * stock_held << endl;
                data << endl;
            }
            throw_out = i;
        }
        if (stock_held != 0) {  //有交易過且有持股
            remain += stock_held * stock_table[throw_out];
            stock_held = 0;
            if (flag != 1) {
                data << "Sell," + days_table[throw_out] + "," << stock_table[throw_out] << "," << big_RSI_table[throw_out][the_best.period_dec] << "," << stock_held;
                data << "," << remain << "," << remain + stock_table[throw_out] * stock_held << endl;
                data << endl;
            }
        }
    }
    // ===============================================================結束輸出每次交易細節
    data.close();
}

void start_train() {
    // ofstream debug;
    // debug.open("debug.csv");
    vector< string > RSI_file = get_file(RSI_path);  //fetch RSI table
    vector< string > stock_file = get_stock_file();  //fectch stock price
    for (int i = 0; i < stock_file.size(); i++) {  //create company folder
        create_folder(stock_file[i], "company");
    }
    vector< string > company = get_folder();
    int companyNum = company.size();
    for (int i = 0; i < companyNum; i++) {  //create train and test folder
        create_folder(company[i], "train");
        create_folder(company[i], "test");
    }
    for (int i = 0; i < companyNum; i++) {  //create SW folder
        cout << company[i] << endl;
        for (int j = 0; j < sizeof(sliding_windows) / sizeof(sliding_windows[0]); j++) {
            create_folder(company[i], sliding_windows[j]);
        }
    }
    for (int company_index = 0; company_index < companyNum; company_index++) {
        cout << "===========================" << stock_file[company_index] << endl;
        int total_days = 0;
        int slideNum = sizeof(sliding_windows) / sizeof(sliding_windows[0]);
        for (int slide = 0; slide < slideNum; slide++) {
            srand(343);
            total_days = store_RSI_and_price(RSI_path + "/" + RSI_file[company_index], price_path + "/" + stock_file[company_index], slide);  //用超大陣列記錄所有RSI及股價
            int interval_cnt = (int)interval_table.size();
            for (int interval_index = 0; interval_index < interval_cnt; interval_index += 2) {
                int earlestExp = 0;
                int earlestGen = 0;
                int theBestGen = 0;
                int bestTimes = 0;
                // debug << "===" + days_table[interval_table[interval_index]] + "~" + days_table[interval_table[interval_index + 1]] + "===" << endl;
                cout << "===" + days_table[interval_table[interval_index]] + "~" + days_table[interval_table[interval_index + 1]] + "===" << endl;
                the_best_ini();
                for (int exp = 0; exp < exp_times; exp++) {
                    // cout << "exp: " << exp + 1 << "   ";
                    cal(interval_index /*, debug*/, earlestGen);
                    if (the_best.RoR < Gbest.RoR) {
                        earlestExp = exp;
                        theBestGen = earlestGen;
                    }
                    the_best_update();
                    // cout << Gbest.RoR << "%" << endl;
                }
                output(interval_index, slide, company[company_index], earlestExp + 1, theBestGen + 1);
                cout << the_best.RoR << "%" << endl;
            }
            interval_table.clear();
        }
        delete[] days_table;
        delete[] stock_table;
        for (int i = 0; i < total_days; i++) {
            delete[] big_RSI_table[i];
        }
        delete[] big_RSI_table;
    }
    // debug.close();
}

void test_period_RoR(string* days, double* price, double** RSITable, string startingDate, string endingDate, int period, int buySignal, int sellSignal, int totalDays) {
    int startingRow = 0;
    for (int i = 0; i < totalDays; i++) {
        if (days[i] == startingDate) {
            startingRow = i;
            break;
        }
    }
    int endingRow = 0;
    for (int i = startingRow; i < totalDays; i++) {
        if (days[i] == endingDate) {
            endingRow = i;
            break;
        }
    }
    cout << days[startingRow] << endl;
    cout << days[endingRow] << endl;

    int stockHeld = 0;
    double remain = 0;
    int flag = 0;  //記錄手上有錢還是有股票
    int sellNum = 0;
    int buyNum = 0;
    // for (int i = startingRow; i <= endingRow - period + 1; i++) { why endingRow - period + 1===============
    for (int i = startingRow; i <= endingRow; i++) {
        if (RSITable[i][period] <= buySignal && stockHeld == 0) {  //買入訊號出現且無持股
            if (flag == 0) {  //等待第一次RSI小於low_bound
                buyNum++;
                stockHeld = TOTAL_CP_LV / price[i];
                remain = TOTAL_CP_LV - price[i] * stockHeld;
                flag = 1;
                cout << "buy: " << days[i] << "," << price[i] << "," << RSITable[i][period] << "," << remain << endl;
            }
            else {
                buyNum++;
                stockHeld = remain / price[i];
                remain -= (double)stockHeld * price[i];
                cout << "buy: " << days[i] << "," << price[i] << "," << RSITable[i][period] << "," << remain << endl;
            }
        }
        else if (RSITable[i][period] >= sellSignal && stockHeld != 0) {  //賣出訊號出現且有持股
            sellNum++;
            remain += (double)stockHeld * price[i];
            stockHeld = 0;
            cout << "sell: " << days[i] << "," << price[i] << "," << RSITable[i][period] << "," << remain << endl;
        }
    }
    if (flag == 0) {
        cout << fixed << setprecision(10) << "0%" << endl;
    }
    else if (stockHeld != 0) {
        sellNum++;
        remain += stockHeld * price[endingRow];
        cout << "sell: " << days[endingRow] << "," << price[endingRow] << "," << RSITable[endingRow][period] << "," << remain << endl;
        cout << fixed << setprecision(10) << ((remain - TOTAL_CP_LV) / TOTAL_CP_LV) * 100 << "%" << endl;
    }
    else {  //有交易過且在最後一天沒有持有股票
        //不做事
        cout << fixed << setprecision(10) << ((remain - TOTAL_CP_LV) / TOTAL_CP_LV) * 100 << "%," << endl;
    }
    cout << "trading times: " << sellNum << endl;
}

void start_test() {
    vector< string > company = get_file(output_path);  //fetch companies name
    /* for (int i = 0; i < company.size(); i++) {
        cout << company[i] << endl;
    } */
    int companyNum = company.size();  //calculate the number of companies
    vector< string > RSI_table = get_file(RSI_path);  //fetch RSI table
    for (int whichCompany = 0; whichCompany < 1; whichCompany++) {
        cout << company[whichCompany] << endl;
        int totalDays = store_RSI_and_price(RSI_path + "/" + RSI_table[whichCompany], price_path + "/" + company[whichCompany] + ".csv", 0);  // store RSI and price
        int slideNum = sizeof(sliding_windows) / sizeof(sliding_windows[0]);  //calculate the number of sliding windows
        for (int slideUse = 1; slideUse < 2; slideUse++) {  //A2A no test period
            // cout << output_path + "/" + company[whichCompany] + "/train/" + sliding_windows[slideUse] << endl;
            vector< string > strategy = get_file(output_path + "/" + company[whichCompany] + "/train/" + sliding_windows[slideUse]);  //fetch strategy files under certain SW
            // cout << company[whichCompany] + ":" + sliding_windows[slideUse] << endl;

            // for (int i = 0; i < strategyNum; i++) {
            //     cout << strategy[i] << endl;
            // }
            int strategyNum = strategy.size();  //calculate number of stategy files
            for (int strategyUse = 0; strategyUse < 1; strategyUse++) {
                string startingDate = strategy[strategyUse].substr(0, 10);  //cut starting date
                string endingDate = strategy[strategyUse].substr(11, 10);  //cut ending date
                string straPath = output_path + "/" + company[whichCompany] + "/train/" + sliding_windows[slideUse] + "/" + strategy[strategyUse];  //strategy file path
                cout << straPath << endl;
                vector< vector< string > > straIn = readData(straPath);
                int period = stod(straIn[9][1]);
                int buySignal = stod(straIn[10][1]);
                int sellSignal = stod(straIn[11][1]);
                test_period_RoR(days_table, stock_table, big_RSI_table, startingDate, endingDate, period, buySignal, sellSignal, totalDays);
            }
        }
        delete[] days_table;
        delete[] stock_table;
        for (int i = 0; i < totalDays; i++) {
            delete[] big_RSI_table[i];
        }
        delete[] big_RSI_table;
    }
}

int main(void) {
    switch (MODE) {
        case 0:
            start_train();
            break;
        case 1:
            start_test();
            break;
        default:
            cout << "Wrong MODE" << endl;
            break;
    }
}