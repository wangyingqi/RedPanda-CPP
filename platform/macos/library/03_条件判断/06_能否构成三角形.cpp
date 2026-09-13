// 能否构成三角形
// 知识点：条件与逻辑
#include <bits/stdc++.h>
using namespace std;

int main() {
    int a,b,c; cin >> a >> b >> c;
    bool ok = a+b>c && a+c>b && b+c>a;
    cout << (ok?"yes":"no") << endl;
    return 0;
}
