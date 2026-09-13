// 数n有几位
// 知识点：循环计数
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n,c=0; cin >> n;
    if(n==0)c=1;
    while(n){ c++; n/=10; }
    cout << c << endl;
    return 0;
}
