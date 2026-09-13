// 分离个位十位百位
// 知识点：除法与取模
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    cin >> n;
    cout << n/100 << " " << n/10%10 << " " << n%10 << endl;
    return 0;
}
