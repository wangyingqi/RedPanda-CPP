// 判断素数函数
// 知识点：bool 函数
#include <bits/stdc++.h>
using namespace std;

bool isPrime(int n) {
    if (n < 2) return false;
    for (int i = 2; (long long)i*i <= n; i++) if (n%i==0) return false;
    return true;
}

int main() {
    int n; cin >> n;
    cout << (isPrime(n) ? "yes":"no") << endl;
    return 0;
}
