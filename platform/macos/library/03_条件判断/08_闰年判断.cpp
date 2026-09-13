// 闰年判断
// 知识点：复合条件
#include <bits/stdc++.h>
using namespace std;

int main() {
    int y; cin >> y;
    cout << (((y%4==0&&y%100!=0)||y%400==0)?"yes":"no") << endl;
    return 0;
}
