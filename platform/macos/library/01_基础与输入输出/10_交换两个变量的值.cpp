// 交换两个变量的值
// 知识点：中间变量
#include <bits/stdc++.h>
using namespace std;

int main() {
    int a, b;
    cin >> a >> b;
    int t = a; a = b; b = t;
    cout << a << " " << b << endl;
    return 0;
}
