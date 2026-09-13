// while 求和
// 知识点：while
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n,i=1,s=0; cin >> n;
    while(i<=n){ s+=i; i++; }
    cout << s << endl;
    return 0;
}
