// 数位反转
// 知识点：取模建数
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n;
    int r = 0;
    while (n>0){ r = r*10 + n%10; n/=10; }
    cout << r << endl;
    return 0;
}
