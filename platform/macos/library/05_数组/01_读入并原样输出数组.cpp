// 读入并原样输出数组
// 知识点：数组遍历
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n; vector<int> a(n);
    for(int i=0;i<n;i++) cin>>a[i];
    for(int i=0;i<n;i++){ cout<<a[i]; if(i<n-1)cout<<" "; }
    cout<<endl;
    return 0;
}
