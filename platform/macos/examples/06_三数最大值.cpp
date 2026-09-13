// 例6：三个数的最大值
#include <bits/stdc++.h>
using namespace std;

int main() {
    int a, b, c;
    cin >> a >> b >> c;
    int mx = a;
    if (b > mx) mx = b;
    if (c > mx) mx = c;
    cout << mx << endl;
    return 0;
}
