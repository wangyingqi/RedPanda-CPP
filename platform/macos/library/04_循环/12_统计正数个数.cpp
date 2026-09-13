// 统计正数个数
// 知识点：计数
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n,c=0; cin >> n;
    for (int i=0;i<n;i++){ int x; cin>>x; if(x>0)c++; }
    cout << c << endl;
    return 0;
}
