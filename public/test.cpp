#include <stdio.h>
#include <math.h>
#include<iostream>
using std::cout;
using std::endl;
int a(double banna,int apple){
    printf("call b\n");
}
int a(int apple,double banna){
    printf("call a\n");
}
void test() {
    static int k=0;
    k++;
    printf("%d\n",k);
}

int main(int argc,char** argv){
    long long int a =1,b=1;
    a= pow(2,15);
    b = pow(2,15);
    a=-a;b=-b;
    a++;b++;
    long long int c = a*b;
    printf("%lld\n",c);
    int d = 0;
    for(int i=16;i<=29;i++)
        d += pow(2,i);
    d++;
    printf("%d\n",d);
    int f = pow(2,15);
    printf("%d\n",f);
}
