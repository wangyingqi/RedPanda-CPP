// 猜数提示(单次)
// 知识点：比较分支
#include <bits/stdc++.h>
using namespace std;

int main() {
    int target=42, g; cin >> g;
    if(g==target) cout<<"right";
    else if(g<target) cout<<"bigger";
    else cout<<"smaller";
    cout<<endl;
    return 0;
}
