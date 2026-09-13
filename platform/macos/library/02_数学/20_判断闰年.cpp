// 判断闰年
// 知识点：逻辑运算
#include <bits/stdc++.h>
using namespace std;

int main() {
    int y; cin >> y;
    bool leap = (y%4==0 && y%100!=0) || (y%400==0);
    cout << (leap ? "yes" : "no") << endl;
    return 0;
}
