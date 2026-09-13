// 数组求和
// 知识点：遍历累加
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n; vector<int> a(n); long long s=0;
    for(int i=0;i<n;i++){ cin>>a[i]; s+=a[i]; }
    cout<<s<<endl;
    return 0;
}
