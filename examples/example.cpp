#include "../include/logfunc.h"

int main() {
    // 例1: ファイル出力とファイル入力（Modern C++版）
    logff("input number:\n");
    int num;
    loginf(num);
    logff("you input number is: ", num, "\n");
    logc("Console output: ", num, "\n");
    
    // 例2: 複数の値を一度に出力
    float f;
    logff("input float number:\n");
    loginc(f);
    logff("you input float number is: ", f, "\n");
    
    // 例3: ストリームスタイルでの出力
    int x = 10, y = 20;
    logff("x=", x, ", y=", y, ", sum=", x + y, "\n");
    
    // 例4: logto関数で特定ファイルに出力
    logto("debug.txt", "Debug info: num=", num, ", f=", f, "\n");
    logto("result.txt", "Result: ", x + y, "\n");
    
    return 0;
}
