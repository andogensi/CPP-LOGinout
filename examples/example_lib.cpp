#include "../include/logfunc_lib.h"

int main() {
    // 例1: ファイル出力とファイル入力（Modern C++版）
    logff("input number:\n");
    int num;
    loginf(num);
    logc("you input number is: ", num, "\n");
    
    // 例2: ファイル出力とコンソール入力
    float f;
    logff("input float number:\n");
    loginc(f);
    logc("you input float number is: ", f, "\n");
    
    return 0;
}
