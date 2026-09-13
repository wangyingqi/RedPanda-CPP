// 各位数字之和
// 知识点：循环取模
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n, s = 0;
    cin >> n;
    while (n > 0) { s += n%10; n /= 10; }
    cout << s << endl;
    return 0;
}
