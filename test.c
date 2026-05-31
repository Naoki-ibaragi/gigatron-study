#include <stdio.h>

void eight_bit_str(int a, int arr[8]) {
    for (int i = 0; i < 8; i++) {
        arr[i] = (a >> i) & 1;
    }
}

int main(void) {
    int arr[8];
    eight_bit_str(10, arr);

    for (int i = 7; i >= 0; i--) {
        printf("%d", arr[i]);
    }
    printf("\n");
}
