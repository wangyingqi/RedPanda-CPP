// BMI 分类
// 知识点：区间判断
#include <bits/stdc++.h>
using namespace std;

int main() {
    double w,h; cin >> w >> h;
    double bmi = w/(h*h);
    if(bmi<18.5)cout<<"thin";else if(bmi<24)cout<<"normal";else cout<<"fat";
    cout<<endl;
    return 0;
}
