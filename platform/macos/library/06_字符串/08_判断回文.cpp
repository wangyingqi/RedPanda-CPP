// 判断回文
// 知识点：双指针
#include <bits/stdc++.h>
using namespace std;

int main() {
    string s; cin >> s;
    string t = s; reverse(t.begin(), t.end());
    cout << (s==t ? "yes":"no") << endl;
    return 0;
}
