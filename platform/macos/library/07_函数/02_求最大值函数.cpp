// 求最大值函数
// 知识点：返回值
#include <bits/stdc++.h>
using namespace std;

int mymax(int a, int b) {
    return a > b ? a : b;
}

int main() {
    int a, b; cin >> a >> b;
    cout << mymax(a, b) << endl;
    return 0;
}
