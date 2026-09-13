// 数组反转输出
// 知识点：倒序遍历
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n; vector<int> a(n);
    for(int i=0;i<n;i++) cin>>a[i];
    for(int i=n-1;i>=0;i--){ cout<<a[i]; if(i)cout<<" "; }
    cout<<endl;
    return 0;
}
