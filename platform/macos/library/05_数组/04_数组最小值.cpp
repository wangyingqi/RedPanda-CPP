// 数组最小值
// 知识点：min_element
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n; vector<int> a(n);
    for(int i=0;i<n;i++) cin>>a[i];
    cout << *min_element(a.begin(),a.end()) << endl;
    return 0;
}
