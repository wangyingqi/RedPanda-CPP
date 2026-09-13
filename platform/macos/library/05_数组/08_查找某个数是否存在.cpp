// 查找某个数是否存在
// 知识点：线性查找
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n; vector<int> a(n);
    for(int i=0;i<n;i++) cin>>a[i];
    int x; cin>>x;
    bool f=false; for(int i=0;i<n;i++) if(a[i]==x){f=true;break;}
    cout << (f?"yes":"no") << endl;
    return 0;
}
