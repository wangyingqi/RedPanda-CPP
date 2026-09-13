// 数组元素累加前缀和
// 知识点：前缀和
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n; vector<int> a(n);
    for(int i=0;i<n;i++) cin>>a[i];
    long long s=0;
    for(int i=0;i<n;i++){ s+=a[i]; cout<<s; if(i<n-1)cout<<" "; }
    cout<<endl;
    return 0;
}
