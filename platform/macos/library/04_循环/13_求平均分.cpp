// 求平均分
// 知识点：累加求平均
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n; double s=0;
    for (int i=0;i<n;i++){ double x; cin>>x; s+=x; }
    cout << s/n << endl;
    return 0;
}
