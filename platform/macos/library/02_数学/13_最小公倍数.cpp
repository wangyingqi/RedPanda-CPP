// 最小公倍数
// 知识点：gcd 推导
#include <bits/stdc++.h>
using namespace std;

int main() {
    int a, b;
    cin >> a >> b;
    cout << (long long)a / __gcd(a,b) * b << endl;
    return 0;
}
