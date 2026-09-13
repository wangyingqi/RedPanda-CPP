// 统计及格人数
// 知识点：综合循环判断
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n,c=0; cin >> n;
    for(int i=0;i<n;i++){ int s; cin>>s; if(s>=60)c++; }
    cout<<c<<endl;
    return 0;
}
