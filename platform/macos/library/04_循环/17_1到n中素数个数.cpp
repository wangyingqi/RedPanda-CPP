// 1到n中素数个数
// 知识点：循环+判断
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n,c=0; cin >> n;
    for (int k=2;k<=n;k++){ bool p=true; for(int i=2;(long long)i*i<=k;i++) if(k%i==0){p=false;break;} if(p)c++; }
    cout<<c<<endl;
    return 0;
}
