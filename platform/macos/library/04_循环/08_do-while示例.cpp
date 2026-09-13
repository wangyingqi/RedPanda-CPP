// do-while 示例
// 知识点：do-while
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n,i=1,s=0; cin >> n;
    do { s+=i; i++; } while(i<=n);
    cout << s << endl;
    return 0;
}
