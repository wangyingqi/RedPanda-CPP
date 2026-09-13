// 统计某字符出现次数
// 知识点：count
#include <bits/stdc++.h>
using namespace std;

int main() {
    string s; cin >> s; char c; cin >> c;
    cout << count(s.begin(), s.end(), c) << endl;
    return 0;
}
