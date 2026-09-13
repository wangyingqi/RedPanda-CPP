// 偶数之和
// 知识点：步长/条件
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n,s=0; cin >> n;
    for (int i=2;i<=n;i+=2) s+=i;
    cout << s << endl;
    return 0;
}
