// 打印所有约数
// 知识点：循环判断
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n;
    for (int i=1;i<=n;i++) if(n%i==0) cout<<i<<" ";
    cout<<endl;
    return 0;
}
