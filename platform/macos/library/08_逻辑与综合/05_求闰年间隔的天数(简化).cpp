// 求闰年间隔的天数(简化)
// 知识点：逻辑综合
#include <bits/stdc++.h>
using namespace std;

int main() {
    int y; cin >> y;
    bool leap=(y%4==0&&y%100!=0)||(y%400==0);
    cout << (leap?366:365) << endl;
    return 0;
}
