// 简单密码校验
// 知识点：字符串比较
#include <bits/stdc++.h>
using namespace std;

int main() {
    string pwd; cin >> pwd;
    cout << (pwd=="123456" ? "correct":"wrong") << endl;
    return 0;
}
