// 求最大公约数函数
// 知识点：递归 gcd
#include <bits/stdc++.h>
using namespace std;

int gcd(int a, int b) {
    return b == 0 ? a : gcd(b, a % b);
}

int main() {
    int a, b; cin >> a >> b;
    cout << gcd(a, b) << endl;
    return 0;
}
