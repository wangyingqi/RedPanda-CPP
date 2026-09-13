// 例14：数组求最大值
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    cin >> n;
    vector<int> a(n);
    for (int i = 0; i < n; i++) cin >> a[i];
    int mx = a[0];
    for (int i = 1; i < n; i++)
        if (a[i] > mx) mx = a[i];
    cout << mx << endl;
    return 0;
}
