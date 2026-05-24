#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>

void printb(unsigned int v) {
  unsigned int mask = (int)1 << (sizeof(v) * CHAR_BIT - 1);
  do putchar(mask & v ? '1' : '0');
  while (mask >>= 1);
}

void garble(uint8_t mem[],int len){
    for (int i=0; i<len; i++) mem[i]=rand();
}

int main(void){
    uint8_t s[100][2];
    srand(time(NULL));
    garble((void*)s,sizeof s);
    for(int i=0; i<100;i++){
      printf("%d %d\n",s[i][0],s[i][1]);
    }

}