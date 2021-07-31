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
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// #include "dirent.h"

using namespace std;
using namespace filesystem;

#define PARTICAL_AMOUNT 10
#define START 2  //股價csv裡的開始欄位
#define COL 4  //股價在第幾COLumn
#define TOTAL_CP_LV 10000000.0

#define MODE 3  //0:train, 1:test, 2:IRR

double _delta = 0.003;
int _exp_times = 50;
int _generation = 1000;

string _starting_date = "2010-01-04";
string _ending_date = "2020-12-31";
string _test_start_y = to_string(stoi(_starting_date.substr(0, 4)) + 1);
string _test_start_m = _starting_date.substr(5, 2);
int _test_length = stoi(_ending_date.substr(0, 4)) - stoi(_starting_date.substr(0, 4));
string _sliding_windows[] = {"A2A", "Y2Y", "Y2H", "Y2Q", "Y2M", "H#", "H2H", "H2Q", "H2M", "Q#", "Q2Q", "Q2M", "M#", "M2M"};

// string _RSI_table_path = "/Users/neo/Desktop/VScode/new training/RSI/all_RSI_table";
// string _price_path = "/Users/neo/Desktop/VScode/new training/RSI/all_price";
// string _output_path = "/Users/neo/Desktop/VScode/new training/RSI/all_sw";
string _RSI_table_path = "D:/stock_info/all_RSI_table";
string _price_path = "D:/stock_info/all_price";
string _output_path = "D:/stock_info/all_sw";

string* _days_table;  //記錄開始日期到結束日期
double** _RSI_table;  //記錄一間公司開始日期到結束日期1~256的RSI
double* _price_table;  //記錄開始日期的股價到結束日期的股價
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
    double buying_signal_dec;  //oversold
    double selling_signal_dec;  //overbought
    double RoR;
    double final_cp_lv;
    int trading_times;
} partical[PARTICAL_AMOUNT], the_best, Gbest, Gworst, Lbest, Lworst;

// void create_folder(string company, string slide_folder) {
//     string s;
//     if (slide_folder == "company") {
//         for (int i = 0; i < 4; i++) {
//             company.pop_back();
//         }
//         s = _output_path + "/" + company;
//     }
//     else if (slide_folder == "train" || slide_folder == "test") {
//         s = _output_path + "/" + company + "/" + slide_folder;
//     }
//     else {
//         s = _output_path + "/" + company + "/train/" + slide_folder;
//         struct stat info;
//         if (stat(s.c_str(), &info) != 0) {
//             cout << "cannot access " << s << endl;
//             if (mkdir(s.c_str(), 0777) == -1) {
//                 cerr << "Error : " << strerror(errno) << endl;
//             }
//             else {
//                 cout << "Directory created" << endl;
//             }
//         }
//         s = _output_path + "/" + company + "/test/" + slide_folder;
//     }
//     struct stat info;
//     if (stat(s.c_str(), &info) != 0) {
//         cout << "cannot access " << s << endl;
//         if (mkdir(s.c_str(), 0777) == -1) {
//             cerr << "Error : " << strerror(errno) << endl;
//         }
//         else {
//             cout << "Directory created" << endl;
//         }
//     }
// }

vector< vector< string > > read_data(string filename) {
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
    dir = opendir(_output_path.c_str());
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
    dir = opendir(_price_path.c_str());
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

void ini_global() {
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

void ini_the_best() {
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

void ini_beta_matrix() {
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
            if (r <= prob_matrix.period[j]) {
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
            if (r <= prob_matrix.buying_signal[j]) {
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
            if (r <= prob_matrix.selling_signal[j]) {
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

void update_local(int partical_num) {
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

void update_global(/*ofstream& debug*/) {
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
    if (Gbest.RoR != 0) {
        for (int i = 0; i < 8; i++) {
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

void update_the_best() {
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
        if (_days_table[i].substr(5, 2) != mon) {
            skip++;
            mon = _days_table[i].substr(5, 2);
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
        start = _days_table[0].substr(5, 2);
        end = _days_table[test_s_row].substr(5, 2);
    }
    else {
        for (int i = test_s_row; i >= -1; i--) {
            if (_days_table[i].substr(5, 2) != mon) {
                skip++;
                mon = _days_table[i].substr(5, 2);
                if (skip == to_jump) {
                    train_start_row = i + 1;
                    start = _days_table[i + 1].substr(5, 2);
                    end = _days_table[test_s_row].substr(5, 2);
                    break;
                }
                i -= 15;
            }
        }
    }
    return to_jump;
}

void find_window_start_end(int table_size, string windowUse, int sliding_type_int) {
    string M[] = {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12"};
    int test_s_row = 0;  //記錄測試期開始row
    for (int i = 230; i < 300; i++) {  //找出測試期開始的row
        if (_days_table[i].substr(0, 4) == _test_start_y) {
            test_s_row = i;
            break;
        }
    }
    int train_start_row = 0;  //記錄訓練期開始的row
    int train_end_row = 0;  //記錄訓練期結束的row
    if (sliding_type_int == 2) {  //判斷year-on-year
        train_start_row = 0;
        string train_end_y = to_string(stoi(_ending_date.substr(0, 4)) - 1);
        for (int i = table_size - 230; i > 0; i--) {
            if (_days_table[i].substr(0, 4) == train_end_y) {
                train_end_row = i;
                break;
            }
        }
        switch (windowUse[0]) {  //做H*
            case 'H': {
                interval_table.push_back(train_start_row);
                string end_H = M[6];
                for (int i = 97, j = 6; i < train_end_row; i++) {
                    if (_days_table[i].substr(5, 2) == end_H) {
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
                    if (_days_table[i].substr(5, 2) == end_Q) {
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
                    if (_days_table[i].substr(5, 2) == end_H) {
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
        string mon = _test_start_m;  //記錄目前iterate到什麼月份
        string start;  //記錄sliding window開始月份
        string end;  //記錄sliding window結束月份
        int s_jump_e = 0;  //從period頭要找period尾
        int e_jump_s = 0;  //從period尾要找下一個period頭
        int skip = 0;  //M[]要跳幾個月份
        switch (windowUse[0]) {  //看是什麼開頭，找出第一個訓練開始的row以及開始月份及結束月份
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
        switch (windowUse[2]) {  //看是什麼結尾，找出最後一個訓練開始的row
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
            if (k % 2 == 1 && _days_table[i].substr(5, 2) == end) {
                interval_table.push_back(i - 1);
                start = M[(train_s_M += skip) % 12];
                end = M[(train_e_M += skip) % 12];
                i -= e_jump_s * 24;
                if (i < 0) {
                    i = 0;
                }
                k++;
            }
            else if (k % 2 == 0 && _days_table[i].substr(5, 2) == start) {
                interval_table.push_back(i);
                i += s_jump_e * 18;
                k++;
            }
        }
        interval_table.push_back(train_end_row);
    }
}

void find_interval(int table_size, int slide) {  //將A2A跟其他分開
    cout << "looking for interval " + _sliding_windows[slide] << endl;
    if (_sliding_windows[slide] == "A2A") {  //直接給A2A的起始與結束row
        interval_table.push_back(0);
        interval_table.push_back(table_size - 1);
    }
    else {  //開始找普通滑動視窗的起始與結束
        find_window_start_end(table_size, _sliding_windows[slide], (int)_sliding_windows[slide].size());
    }
    for (int i = 0; i < interval_table.size(); i += 2) {
        cout << _days_table[interval_table[i]] + "~" + _days_table[interval_table[i + 1]] << endl;
    }
}

int store_RSI_and_price(string RSI_table_path, string stock_file_path, int slide) {
    vector< vector< string > > RSI_table_in = read_data(RSI_table_path);
    int table_size = (int)RSI_table_in.size();
    _days_table = new string[table_size];
    for (int i = 0; i < table_size; i++) {
        _days_table[i] = RSI_table_in[i][0];
    }
    _RSI_table = new double*[table_size];
    for (int i = 0; i < table_size; i++) {
        _RSI_table[i] = new double[257];
        for (int j = 1; j < 257; j++) {
            _RSI_table[i][j] = stod(RSI_table_in[i][j]);
        }
    }
    vector< vector< string > > stock_price_in = read_data(stock_file_path);
    int price_size = (int)stock_price_in.size();
    int startingRow = 0;
    int ending_row = 0;
    for (int i = 0; i < price_size; i++) {
        if (stock_price_in[i][0] == _starting_date) {
            startingRow = i;
            for (; i < price_size; i++) {
                if (stock_price_in[i][0] == _ending_date) {
                    ending_row = i;
                    break;
                }
            }
            break;
        }
    }
    _price_table = new double[table_size];
    for (int i = 0, j = startingRow; j <= ending_row; i++, j++) {
        _price_table[i] = stod(stock_price_in[j][COL]);
    }
    if (MODE == 0) {
        find_interval(table_size, slide);  //找出滑動間格
    }
    return table_size;
}

void cal_train_RoR(int interval_index /*, ofstream& debug*/, int& earlestGen, int gen) {
    int stock_held = 0;
    double remain = TOTAL_CP_LV;
    int flag = 0;  //記錄是否有交易過
    for (int partical_num = 0; partical_num < PARTICAL_AMOUNT; partical_num++) {
        stock_held = 0;
        remain = TOTAL_CP_LV;
        flag = 0;
        // ===============================================================開始計算報酬率
        for (int i = interval_table[interval_index]; i <= interval_table[interval_index + 1]; i++) {
            if (_RSI_table[i][partical[partical_num].period_dec] <= partical[partical_num].buying_signal_dec && stock_held == 0) {  //買入訊號出現且無持股
                stock_held = remain / _price_table[i];
                remain = remain - _price_table[i] * stock_held;
                flag = 1;
                partical[partical_num].trading_times++;
                if (i == interval_table[interval_index + 1]) {
                    partical[partical_num].trading_times--;
                }
            }
            else if (_RSI_table[i][partical[partical_num].period_dec] >= partical[partical_num].selling_signal_dec && stock_held != 0) {  //賣出訊號出現且有持股
                remain += (double)stock_held * _price_table[i];
                stock_held = 0;
            }
        }
        if (flag == 0) {  //沒有交易過
            partical[partical_num].RoR = 0;
            partical[partical_num].final_cp_lv = 0;
        }
        else if (stock_held != 0) {  //有交易過且有持股
            remain += stock_held * _price_table[interval_table[interval_index + 1]];
            partical[partical_num].RoR = ((remain - TOTAL_CP_LV) / TOTAL_CP_LV) * 100;
            partical[partical_num].final_cp_lv = remain;
        }
        else {  //有交易過但沒有持股
            partical[partical_num].RoR = ((remain - TOTAL_CP_LV) / TOTAL_CP_LV) * 100;
            partical[partical_num].final_cp_lv = remain;
        }
        // ===============================================================報酬率計算結束
        update_local(partical_num);
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
    update_global(/*debug*/);
}

void cal(int interval_index /*, ofstream& debug*/, int& earlestGen) {
    ini_global();
    ini_beta_matrix();
    for (int gen = 0; gen < _generation; gen++) {
        // debug << "gen: " << gen << endl;
        ini_local();
        measure(/*debug*/);
        bi_to_dec();
        cal_train_RoR(interval_index /*, debug*/, earlestGen, gen);
    }
    // cout << the_best.RoR << "%" << endl;
}

void output(int interval_index, int slide, string company, int earlestExp, int earlestGen) {
    ofstream data;
    data.open(_output_path + "/" + company + "/train/" + _sliding_windows[slide] + "/" +
              _days_table[interval_table[interval_index]] + "_" + _days_table[interval_table[interval_index + 1]] + ".csv");
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
            if (_RSI_table[i][the_best.period_dec] <= the_best.buying_signal_dec && stock_held == 0) {  //買入訊號出現且無持股
                stock_held = remain / _price_table[i];
                remain = remain - _price_table[i] * stock_held;
                if (i != interval_table[interval_index + 1]) {
                    data << "Buy," + _days_table[i] + "," << _price_table[i] << "," << _RSI_table[i][the_best.period_dec] << "," << stock_held;
                    data << "," << remain << "," << remain + _price_table[i] * stock_held << endl;
                }
                if (i == interval_table[interval_index + 1]) {
                    flag = 1;
                }
            }
            else if (_RSI_table[i][the_best.period_dec] >= the_best.selling_signal_dec && stock_held != 0) {  //賣出訊號出現且有持股
                remain += (double)stock_held * _price_table[i];
                stock_held = 0;
                data << "Sell," + _days_table[i] + "," << _price_table[i] << "," << _RSI_table[i][the_best.period_dec] << "," << stock_held;
                data << "," << remain << "," << remain + _price_table[i] * stock_held << endl;
                data << endl;
            }
            throw_out = i;
        }
        if (stock_held != 0) {  //有交易過且有持股
            remain += stock_held * _price_table[throw_out];
            stock_held = 0;
            if (flag != 1) {
                data << "Sell," + _days_table[throw_out] + "," << _price_table[throw_out] << "," << _RSI_table[throw_out][the_best.period_dec] << "," << stock_held;
                data << "," << remain << "," << remain + _price_table[throw_out] * stock_held << endl;
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
    vector< string > RSI_file = get_file(_RSI_table_path);  //get RSI table
    vector< string > stock_file = get_stock_file();  //get stock price
    // for (int i = 0; i < stock_file.size(); i++) {  //create company folder
    //     create_folder(stock_file[i], "company");
    // }
    vector< string > company = get_folder();
    int companyNum = company.size();
    // for (int i = 0; i < companyNum; i++) {  //create train and test folder
    //     create_folder(company[i], "train");
    //     create_folder(company[i], "test");
    // }
    // for (int i = 0; i < companyNum; i++) {  //create windowUse folder
    //     cout << company[i] << endl;
    //     for (int j = 0; j < sizeof(_sliding_windows) / sizeof(_sliding_windows[0]); j++) {
    //         create_folder(company[i], _sliding_windows[j]);
    //     }
    // }
    for (int company_index = 0; company_index < 1; company_index++) {
        cout << "===========================" << stock_file[company_index] << endl;
        int total_days = 0;
        int windowNum = sizeof(_sliding_windows) / sizeof(_sliding_windows[0]);
        for (int slide = 0; slide < windowNum; slide++) {
            srand(343);
            total_days = store_RSI_and_price(_RSI_table_path + "/" + RSI_file[company_index], _price_path + "/" + stock_file[company_index], slide);  //用超大陣列記錄所有RSI及股價
            int interval_cnt = (int)interval_table.size();
            for (int interval_index = 0; interval_index < interval_cnt; interval_index += 2) {
                int earlestExp = 0;
                int earlestGen = 0;
                int theBestGen = 0;
                int bestTimes = 0;
                // debug << "===" + _days_table[interval_table[interval_index]] + "~" + _days_table[interval_table[interval_index + 1]] + "===" << endl;
                cout << "===" + _days_table[interval_table[interval_index]] + "~" + _days_table[interval_table[interval_index + 1]] + "===" << endl;
                ini_the_best();
                for (int exp = 0; exp < _exp_times; exp++) {
                    // cout << "exp: " << exp + 1 << "   ";
                    cal(interval_index /*, debug*/, earlestGen);
                    if (the_best.RoR < Gbest.RoR) {
                        earlestExp = exp;
                        theBestGen = earlestGen;
                    }
                    update_the_best();
                    // cout << Gbest.RoR << "%" << endl;
                }
                output(interval_index, slide, company[company_index], earlestExp + 1, theBestGen + 1);
                cout << the_best.RoR << "%" << endl;
            }
            interval_table.clear();
        }
        delete[] _days_table;
        delete[] _price_table;
        for (int i = 0; i < total_days; i++) {
            delete[] _RSI_table[i];
        }
        delete[] _RSI_table;
    }
    // debug.close();
}

void output_test_file(string outputPath, string startDate, string endDate, int period, double buySignal, double sellSignal, int tradeNum, double returnRate, vector< string > tradeReord) {
    ofstream test;
    test.open(outputPath + "/" + startDate + "_" + endDate + ".csv");
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
        // for (int j = 0; j < tradeReord[i].size(); j++) {
        test << tradeReord[i] << endl;
        // }
        // if (i % 2 == 1) {
        //     test << endl;
        // }
    }
    test.close();
}

void cal_test_RoR(string startDate, string endDate, int period, int buySignal, int sellSignal, int totalDays, string outputPath) {
    int startingRow = 0;
    for (int i = 0; i < totalDays; i++) {
        if (_days_table[i] == startDate) {
            startingRow = i;
            break;
        }
    }
    int endingRow = 0;
    for (int i = startingRow; i < totalDays; i++) {
        if (_days_table[i] == endDate) {
            endingRow = i;
            break;
        }
    }
    // cout << _days_table[startingRow] << endl;
    // cout << _days_table[endingRow] << endl;

    int stockHold = 0;
    double remain = TOTAL_CP_LV;
    // int flag = 0;  //記錄手上有錢還是有股票
    int sellNum = 0;
    int buyNum = 0;
    double returnRate = 0;
    // vector< vector< string > > tradeRecord;
    vector< string > tradeRecord;

    if (period != 0) {
        for (int i = startingRow; i <= endingRow; i++) {
            // vector< string > oneTradeRecord;
            if (_RSI_table[i][period] <= buySignal && stockHold == 0 && i < endingRow) {  //買入訊號出現且無持股
                // if (flag == 0) {  //等待第一次RSI小於low_bound
                //     buyNum++;
                //     stockHold = TOTAL_CP_LV / _price_table[i];
                //     remain = TOTAL_CP_LV - _price_table[i] * stockHold;
                //     flag = 1;
                //     oneTradeRecord.push_back("buy," + _days_table[i] + "," + to_string(_price_table[i]) + "," + to_string(_RSI_table[i][period]) + "," + to_string(stockHold) + "," + to_string(remain) + "," + to_string(remain + _price_table[i] * stockHold));
                //     // tmp = "buy " + _days_table[i] + "," + to_string(_price_table[i]) + "," + to_string(_RSI_table[i][period]) + "," + to_string(remain);
                //     // cout << "buy: " << _days_table[i] << "," << _price_table[i] << "," << _RSI_table[i][period] << "," << remain << endl;
                // }
                // else {
                // flag = 1;
                buyNum++;
                stockHold = remain / _price_table[i];
                remain -= (double)stockHold * _price_table[i];
                // oneTradeRecord.push_back("buy," + _days_table[i] + "," + to_string(_price_table[i]) + "," + to_string(_RSI_table[i][period]) + "," + to_string(stockHold) + "," + to_string(remain) + "," + to_string(remain + _price_table[i] * stockHold));
                // tmp = "buy " + _days_table[i] + "," + to_string(_price_table[i]) + "," + to_string(_RSI_table[i][period]) + "," + to_string(remain);
                // cout << "buy: " << _days_table[i] << "," << _price_table[i] << "," << _RSI_table[i][period] << "," << remain << endl;
                // }
                // oneTradeRecord.push_back(tmp);
                tradeRecord.push_back("buy," + _days_table[i] + "," + to_string(_price_table[i]) + "," + to_string(_RSI_table[i][period]) + "," + to_string(stockHold) + "," + to_string(remain) + "," + to_string(remain + _price_table[i] * stockHold));
                // oneTradeRecord.clear();
            }
            else if ((_RSI_table[i][period] >= sellSignal && stockHold != 0) || (stockHold != 0 && i == endingRow)) {  //賣出訊號出現且有持股
                sellNum++;
                remain += (double)stockHold * _price_table[i];
                stockHold = 0;
                // oneTradeRecord.push_back("sell," + _days_table[i] + "," + to_string(_price_table[i]) + "," + to_string(_RSI_table[i][period]) + "," + to_string(stockHold) + "," + to_string(remain) + "," + to_string(remain + _price_table[i] * stockHold));
                tradeRecord.push_back("sell," + _days_table[i] + "," + to_string(_price_table[i]) + "," + to_string(_RSI_table[i][period]) + "," + to_string(stockHold) + "," + to_string(remain) + "," + to_string(remain + _price_table[i] * stockHold));
                // oneTradeRecord.clear();
                // tmp = "sell " + _days_table[i] + "," + to_string(_price_table[i]) + "," + to_string(_RSI_table[i][period]) + "," + to_string(remain);
                // oneTradeRecord.push_back(tmp);
                // cout << "sell: " << _days_table[i] << "," << _price_table[i] << "," << _RSI_table[i][period] << "," << remain << endl;
            }
        }
    }
    //     sellNum++;
    // if (stockHold != 0) {
    //     remain += stockHold * _price_table[endingRow];
    //     stockHold = 0;
    //     oneTradeRecord.push_back("sell," + _days_table[endingRow] + "," + to_string(_price_table[endingRow]) + "," + to_string(_RSI_table[endingRow][period]) + "," + to_string(stockHold) + "," + to_string(remain) + "," + to_string(remain + _price_table[endingRow] * stockHold));
    //     tradeRecord.push_back(oneTradeRecord);
    //     // cout << "sell: " << _days_table[endingRow] << "," << _price_table[endingRow] << "," << _RSI_table[endingRow][period] << "," << remain << endl;
    //     // cout << fixed << setprecision(10) << ((remain - TOTAL_CP_LV) / TOTAL_CP_LV) * 100 << "%" << endl;
    // }
    returnRate = (remain - TOTAL_CP_LV) / TOTAL_CP_LV;
    // cout << returnRate * 100 << endl;
    // cout << "trading times: " << sellNum << endl;
    output_test_file(outputPath, startDate, endDate, period, buySignal, sellSignal, sellNum, returnRate, tradeRecord);
}

vector< string > find_test_interval(char interval, int totalDays) {
    vector< string > testInterval;
    int testStartRow = 0;
    for (int i = 0; i < totalDays; i++) {
        if (_days_table[i].substr(0, 4) == _test_start_y) {
            testStartRow = i;
            break;
        }
    }
    string month[] = {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12"};
    testInterval.push_back(_days_table[testStartRow]);
    int findPeriodEnd = 1;  //表示現在要記錄開始日期還是結束日期
    switch (interval) {
        case 'Y': {
            int nextPeriodStartRow = testStartRow;
            for (; nextPeriodStartRow < totalDays;) {
                if (findPeriodEnd == 1) {
                    string periodEndDate = _days_table[nextPeriodStartRow].substr(0, 4);
                    for (; nextPeriodStartRow < totalDays; nextPeriodStartRow++) {
                        if (_days_table[nextPeriodStartRow].substr(0, 4) != periodEndDate) {
                            testInterval.push_back(_days_table[nextPeriodStartRow - 1]);
                            findPeriodEnd = 0;
                            break;
                        }
                    }
                }
                else {
                    testInterval.push_back(_days_table[nextPeriodStartRow]);
                    findPeriodEnd = 1;
                }
            }
            testInterval.push_back(_days_table[nextPeriodStartRow - 1]);
            break;
        }
        case 'H': {
            int nextPeriodStartIndex = 6;
            int nextPeriodStartRow = testStartRow;
            for (; nextPeriodStartRow < totalDays;) {
                if (findPeriodEnd == 1) {
                    for (; nextPeriodStartRow < totalDays; nextPeriodStartRow++) {
                        if (_days_table[nextPeriodStartRow].substr(5, 2) == month[nextPeriodStartIndex]) {
                            testInterval.push_back(_days_table[nextPeriodStartRow - 1]);
                            findPeriodEnd = 0;
                            break;
                        }
                    }
                }
                else {
                    testInterval.push_back(_days_table[nextPeriodStartRow]);
                    nextPeriodStartIndex = (nextPeriodStartIndex + 6) % 12;
                    findPeriodEnd = 1;
                }
            }
            testInterval.push_back(_days_table[nextPeriodStartRow - 1]);
            break;
        }
        case 'Q': {
            int nextPeriodStartIndex = 3;
            int nextPeriodStartRow = testStartRow;
            for (; nextPeriodStartRow < totalDays;) {
                if (findPeriodEnd == 1) {
                    for (; nextPeriodStartRow < totalDays; nextPeriodStartRow++) {
                        if (_days_table[nextPeriodStartRow].substr(5, 2) == month[nextPeriodStartIndex]) {
                            testInterval.push_back(_days_table[nextPeriodStartRow - 1]);
                            findPeriodEnd = 0;
                            break;
                        }
                    }
                }
                else {
                    testInterval.push_back(_days_table[nextPeriodStartRow]);
                    nextPeriodStartIndex = (nextPeriodStartIndex + 3) % 12;
                    findPeriodEnd = 1;
                }
            }
            testInterval.push_back(_days_table[nextPeriodStartRow - 1]);
            break;
        }
        case 'M': {
            int nextPeriodStartIndex = 1;
            int nextPeriodStartRow = testStartRow;
            for (; nextPeriodStartRow < totalDays;) {
                if (findPeriodEnd == 1) {
                    for (; nextPeriodStartRow < totalDays; nextPeriodStartRow++) {
                        if (_days_table[nextPeriodStartRow].substr(5, 2) == month[nextPeriodStartIndex]) {
                            testInterval.push_back(_days_table[nextPeriodStartRow - 1]);
                            findPeriodEnd = 0;
                            break;
                        }
                    }
                }
                else {
                    testInterval.push_back(_days_table[nextPeriodStartRow]);
                    nextPeriodStartIndex = (nextPeriodStartIndex + 1) % 12;
                    findPeriodEnd = 1;
                }
            }
            testInterval.push_back(_days_table[nextPeriodStartRow - 1]);
            break;
        }
    }
    return testInterval;
}

void start_test() {
    vector< string > company = get_file(_output_path);  //get companies name
    /* for (int i = 0; i < company.size(); i++) {
        cout << company[i] << endl;
    } */
    int companyNum = company.size();
    vector< string > RSI_table = get_file(_RSI_table_path);  //get RSI table
    for (int whichCompany = 0; whichCompany < 1; whichCompany++) {
        cout << "===========================" + company[whichCompany] << endl;
        int totalDays = store_RSI_and_price(_RSI_table_path + "/" + RSI_table[whichCompany], _price_path + "/" + company[whichCompany] + ".csv", 0);
        int windowNum = sizeof(_sliding_windows) / sizeof(_sliding_windows[0]) - 1;  //No A2A
        for (int windowUse = 1; windowUse < windowNum; windowUse++) {  //No A2A
            cout << company[whichCompany] + ":" + _sliding_windows[windowUse] << endl;
            vector< string > strategy = get_file(_output_path + "/" + company[whichCompany] + "/train/" + _sliding_windows[windowUse]);  //get strategy files
            // for (int i = 0; i < strategy.size(); i++) {
            //     cout << strategy[i] << endl;
            // }
            vector< string > testInterval;
            if (_sliding_windows[windowUse].length() == 3) {  //一般測試期區間
                testInterval = find_test_interval(_sliding_windows[windowUse][2], totalDays);
            }
            else {  //年對年
                testInterval = find_test_interval(_sliding_windows[windowUse][0], totalDays);
            }
            string outputPath = _output_path + "/" + company[whichCompany] + "/test/" + _sliding_windows[windowUse];
            int strategyNum = strategy.size();
            for (int strategyUse = 0; strategyUse < strategyNum; strategyUse++) {
                string strategyPath = _output_path + "/" + company[whichCompany] + "/train/" + _sliding_windows[windowUse] + "/" + strategy[strategyUse];
                // cout << strategyPath << endl;
                vector< vector< string > > strategyRead = read_data(strategyPath);
                int period = stod(strategyRead[9][1]);
                int buySignal = stod(strategyRead[10][1]);
                int sellSignal = stod(strategyRead[11][1]);
                // cout << testInterval[strategyUse * 2] + "~" + testInterval[strategyUse * 2 + 1] << endl;
                cal_test_RoR(testInterval[strategyUse * 2], testInterval[strategyUse * 2 + 1], period, buySignal, sellSignal, totalDays, outputPath);
            }
        }
        delete[] _days_table;
        delete[] _price_table;
        for (int i = 0; i < totalDays; i++) {
            delete[] _RSI_table[i];
        }
        delete[] _RSI_table;
    }
}

void cal_IRR() {
    vector< string > company = get_file(_output_path);  //公司名稱
    int companyNum = company.size();
    for (int whichCompany = 0; whichCompany < companyNum; whichCompany++) {
        ofstream out;
        out.open(company[whichCompany] + ".csv");
        vector< string > window = get_file(_output_path + "/" + company[whichCompany] + "/test");  //視窗名稱
        int windowNum = window.size();
        for (int whichWindow = 1; whichWindow < windowNum; whichWindow++) {  //No A2A
            double TotalRate = 0;
            out << "====================" + window[whichWindow] + "====================" << endl;
            vector< string > strategy = get_file(_output_path + "/" + company[whichCompany] + "/test/" + window[whichWindow]);
            int strategyNum = strategy.size();
            for (int strategys = 0; strategys < strategyNum; strategys++) {
                vector< vector< string > > file = read_data(_output_path + "/" + company[whichCompany] + "/test/" + window[whichWindow] + "/" + strategy[strategys]);  //視窗策略
                out << strategy[strategys] + "," + file[9][1] + "," + file[10][1] + "," + file[11][1] + "," + file[13][1] << endl;
                if (strategys == 0) {
                    TotalRate = stod(file[13][1]) / 100 + 1;
                }
                else {
                    TotalRate = TotalRate * (stod(file[13][1]) / 100 + 1);
                }
            }
            double IRR = pow(TotalRate, (double)1 / _test_length) - 1;  //計算年化報酬
            TotalRate--;
            out << ",,,,,," + window[whichWindow] + "," + to_string(TotalRate) + "," + to_string(IRR) << endl;
        }
        out.close();
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
        case 2:
            cal_IRR();
            break;
        default:
            cout << "Wrong MODE" << endl;
            break;
    }
}