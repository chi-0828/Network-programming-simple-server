#include <stdio.h>
int main(int argc, char** argv) {
    printf("start .....\n");
    printf("break point\n");
    asm("int3\n");
    printf("end........\n");
}