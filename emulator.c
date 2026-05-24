#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    uint16_t PC;
    uint8_t IR, D, AC, X, Y, OUT, undef;
    /*
    PC:プログラムカウンタ
    IR:命令(ROMから読み込まれた命令の1byte目)
    D:データレジスタ
    AC:アキュムレータ
    X,Y:Xレジスタ,Yレジスタ
    OUT:出力レジスタ
    undef:未定義状態(ノイズの再現用) 
    */
}CpuState;

// ROM 64kb, RAM 32kb
uint8_t ROM[1<<16][2], RAM[1<<15], IN=0xff;

CpuState cpuCycle(const CpuState S){
    CpuState T = S; //前のcpu状態を引き継ぐ
   
    //ROMからの値のフェッチ
    T.IR = ROM[S.PC][0];
    T.D = ROM[S.PC][1];

    int ins = S.IR >> 5; //instruction decoderへ接続 実回路：74HCT138への入力
    int mod = (S.IR >> 2) & 7; //mode decoder, condition decoderへ接続 74HCT138への入力
    int bus = S.IR&3; //bus access decoderへ接続 74HCT138への入力

    int W = (ins==6); //RAM書き込み判定
    int J = (ins==7); //命令アドレスをジャンプするか

    uint8_t lo=S.D, hi=0, *to=NULL; //Mode選択

    int incX=0;

    if(!J)
        switch (mod){
            #define E(p) (W?0:p)
            case 0: to=E(&T.AC); break;
            case 1: to=E(&T.AC); lo=S.X; break; //lo:RAM下位8bitアドレス(命令の2byte目(Dレジスタ)またはxレジスタの値を格納、hi:RAM上位8bitアドレス 0またはYレジスタの値を格納
            case 2: to=E(&T.AC); hi=S.Y; break;
            case 3: to=E(&T.AC); lo=S.X; hi=S.Y; break;
            case 4: to=&T.X; break;
            case 5: to=&T.Y; break;
            case 6: to=E(&T.OUT); break;
            case 7: to=E(&T.OUT); lo=S.X; hi=S.Y; incX=1; break;
        }
    uint16_t addr = (hi << 8) | lo;

    int B = S.undef;
    switch(bus){
        case 0: B=S.D; break;
        case 1: if (!W) B = RAM[addr&0x7fff]; break;
        case 2: B=S.AC; break;
        case 3: B=IN; break;
    }

    if(W) RAM[addr&0x7fff]=B;

    uint8_t ALU;
    switch(ins){
        case 0: ALU= B; break; //LD
        case 1: ALU=S.AC & B; break; //ANDA
        case 2: ALU=S.AC | B; break; //ORA
        case 3: ALU=S.AC^B; break; //XORA
        case 4: ALU=S.AC + B; break; // ADDA
        case 5: ALU = S.AC -B; break; //SUBA
        case 6: ALU = S.AC; break; //ST
        case 7: ALU = -S.AC; break; //Bcc/JMP
    }

    if (to) *to=ALU; // load value into register
    if (incX) T.X = S.X + 1;

    T.PC = S.PC+1; //Next instruction
    if(J){
        if (mod!=0) {
            int cond = (S.AC>>7) + 2*(S.AC==0);
            if (mod & (1 << cond))
                T.PC = (S.PC &0xff00) | B;
        }else
            T.PC = (S.Y << 8) | B;
    }
    return T;
}

void garble(uint8_t mem[],int len){
    for (int i=0; i<len; i++) mem[i]=rand();
}

int main(void){
    CpuState S;
    srand(time(NULL));

    garble((void*)ROM, sizeof ROM);
    garble((void*)RAM, sizeof RAM);
    garble((void*)&S, sizeof S);

    FILE *fp = fopen("./gigatron-rom/ROMv6.rom","rb");
    if (!fp){
        fprintf(stderr,"Error: failed to open ROM file\n");
        exit(EXIT_FAILURE);
    }
    fread(ROM, 1, sizeof ROM, fp);
    fclose(fp);

    int vgaX=0, vgaY=0;
    for (long long t=-2; ;t++){
        if (t<0) S.PC = 0;

        CpuState T = cpuCycle(S);

        int hSync = (T.OUT & 0x40) - (S.OUT & 0x40);
        int vSync = (T.OUT & 0x80) - (S.OUT & 0x80);
        if (vSync < 0) vgaY = -36;
        if (vgaX++ < 200){
            if (hSync) putchar ('|');
            else if (vgaX == 200) putchar('>');
            else if (-S.OUT & 0x80) putchar('^');
            else putchar(32 + (S.OUT & 63));
        }
        if (hSync > 0){
            printf("%s line %d xout %02x t %0.3f\n",
            vgaX!=200 ? "-":"",vgaY, T.AC, t/6.250e+06);
            vgaX = 0;
            vgaY++;
            T.undef = rand() & 0xff;
        }
        S=T;
    }
    return 0;
}

