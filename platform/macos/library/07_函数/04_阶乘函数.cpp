// 阶乘函数
// 知识点：循环封装
#include <bits/stdc++.h>
using namespace std;

long long fact(int n) {
    long long f = 1;
    for (int i = 1; i <= n; i++) f *= i;
    return f;
}

int main() {
    int n; cin >> n;
    cout << fact(n) << endl;
    return 0;
}
