// 统计某数出现次数
// 知识点：计数
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n; vector<int> a(n);
    for(int i=0;i<n;i++) cin>>a[i];
    int x; cin>>x; int c=0;
    for(int i=0;i<n;i++) if(a[i]==x)c++;
    cout<<c<<endl;
    return 0;
}
