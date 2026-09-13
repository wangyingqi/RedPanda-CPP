// 反转数字
// 知识点：循环取模
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n,r=0; cin >> n;
    while(n){ r=r*10+n%10; n/=10; }
    cout << r << endl;
    return 0;
}
