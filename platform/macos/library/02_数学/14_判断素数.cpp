// 判断素数
// 知识点：试除法
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n;
    bool p = n >= 2;
    for (int i = 2; (long long)i*i <= n; i++) if (n%i==0) { p=false; break; }
    cout << (p ? "yes" : "no") << endl;
    return 0;
}
