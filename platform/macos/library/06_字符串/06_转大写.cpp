// 转大写
// 知识点：toupper
#include <bits/stdc++.h>
using namespace std;

int main() {
    string s; cin >> s;
    for (char &c : s) c = toupper(c);
    cout << s << endl;
    return 0;
}
