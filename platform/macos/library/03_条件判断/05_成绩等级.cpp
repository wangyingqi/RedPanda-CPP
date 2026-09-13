// 成绩等级
// 知识点：if-else 链
#include <bits/stdc++.h>
using namespace std;

int main() {
    int s; cin >> s;
    if(s>=90)cout<<"A";else if(s>=80)cout<<"B";else if(s>=70)cout<<"C";else if(s>=60)cout<<"D";else cout<<"E";
    cout<<endl;
    return 0;
}
