// 例9：1 到 n 求和（for 循环）
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    cin >> n;
    int sum = 0;
    for (int i = 1; i <= n; i++)
        sum += i;
    cout << sum << endl;
    return 0;
}
