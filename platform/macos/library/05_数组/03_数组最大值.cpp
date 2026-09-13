// 数组最大值
// 知识点：打擂台
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n; vector<int> a(n);
    for(int i=0;i<n;i++) cin>>a[i];
    cout << *max_element(a.begin(),a.end()) << endl;
    return 0;
}
