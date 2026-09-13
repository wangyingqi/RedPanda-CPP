// 统计元音个数
// 知识点：字符判断
#include <bits/stdc++.h>
using namespace std;

int main() {
    string s; cin >> s; int c=0;
    for(char ch: s){ char x=tolower(ch); if(x=='a'||x=='e'||x=='i'||x=='o'||x=='u') c++; }
    cout << c << endl;
    return 0;
}
