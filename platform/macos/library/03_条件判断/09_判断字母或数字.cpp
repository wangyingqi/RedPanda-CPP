// 判断字母或数字
// 知识点：字符范围
#include <bits/stdc++.h>
using namespace std;

int main() {
    char c; cin >> c;
    if (c>='0'&&c<='9') cout << "digit";
    else if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')) cout << "letter";
    else cout << "other";
    cout << endl;
    return 0;
}
