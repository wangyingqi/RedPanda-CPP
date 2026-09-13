// 递归求阶乘
// 知识点：递归
#include <bits/stdc++.h>
using namespace std;

long long fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}

int main() {
    int n; cin >> n;
    cout << fact(n) << endl;
    return 0;
}
