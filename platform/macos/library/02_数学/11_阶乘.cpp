// 阶乘
// 知识点：累乘
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    cin >> n;
    long long f = 1;
    for (int i = 1; i <= n; i++) f *= i;
    cout << f << endl;
    return 0;
}
