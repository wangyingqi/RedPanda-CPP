// 引用交换
// 知识点：引用参数
#include <bits/stdc++.h>
using namespace std;

void myswap(int &a, int &b) {
    int t = a; a = b; b = t;
}

int main() {
    int a, b; cin >> a >> b;
    myswap(a, b);
    cout << a << " " << b << endl;
    return 0;
}
