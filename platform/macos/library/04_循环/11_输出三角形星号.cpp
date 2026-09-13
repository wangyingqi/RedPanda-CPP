// 输出三角形星号
// 知识点：递增内层
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n; cin >> n;
    for (int i=1;i<=n;i++){ for(int j=0;j<i;j++) cout<<"*"; cout<<endl; }
    return 0;
}
