// 例13：斐波那契数列第 n 项（1,1,2,3,5,…）
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    cin >> n;
    long long a = 1, b = 1;
    for (int i = 3; i <= n; i++) {
        long long c = a + b;
        a = b;
        b = c;
    }
    cout << (n <= 2 ? 1 : b) << endl;
    return 0;
}
