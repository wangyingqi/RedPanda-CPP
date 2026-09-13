// 求n个数的最大值
// 知识点：打擂台
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n; int mx;
    for (int i=0;i<n;i++){ int x; cin>>x; if(i==0||x>mx) mx=x; }
    cout << mx << endl;
    return 0;
}
