// 幂运算
// 知识点：循环累乘
#include <bits/stdc++.h>
using namespace std;

int main() {
    int a, b;
    cin >> a >> b;
    long long r = 1;
    for (int i = 0; i < b; i++) r *= a;
    cout << r << endl;
    return 0;
}
