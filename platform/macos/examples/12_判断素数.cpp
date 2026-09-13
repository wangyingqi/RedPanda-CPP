// 例12：判断素数
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    cin >> n;
    bool isPrime = true;
    if (n < 2) isPrime = false;
    for (int i = 2; (long long)i * i <= n; i++) {
        if (n % i == 0) { isPrime = false; break; }
    }
    cout << (isPrime ? "yes" : "no") << endl;
    return 0;
}
