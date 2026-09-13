// 求和到n的平方和
// 知识点：累加
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n;
    long long s=0;
    for (int i=1;i<=n;i++) s += (long long)i*i;
    cout << s << endl;
    return 0;
}
