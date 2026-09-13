// 数字字符转整数
// 知识点：字符运算
#include <bits/stdc++.h>
using namespace std;

int main() {
    string s; cin >> s; int v=0;
    for(char c: s) v = v*10 + (c-'0');
    cout << v << endl;
    return 0;
}
