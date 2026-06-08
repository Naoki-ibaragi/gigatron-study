#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void eight_bit_str(int a, int arr[8]) {
    for (int i = 0; i < 8; i++) {
        arr[i] = (a >> i) & 1;
    }
}

void sixteen_bit_str(int a, int arr[16]) {
    for (int i = 0; i < 16; i++) {
        arr[i] = (a >> i) & 1;
    }
}

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

FILE *debug_fp = NULL;

CpuState cpuCycle(const CpuState S,long long t){
    CpuState T = S; //前のcpu状態を引き継ぐ
   
    //ROMからの値のフェッチ
    T.IR = ROM[S.PC][0];
    T.D = ROM[S.PC][1];


    int ins = S.IR >> 5; //instruction decoderへ接続 実回路：74HCT138への入力
    int mod = (S.IR >> 2) & 7; //mode decoder, condition decoderへ接続 74HCT138への入力
    int bus = S.IR&3; //bus access decoderへ接続 74HCT138への入力

    int W = (ins==6); //RAM書き込み判定(Operation DecoderのSTOREが0出力)
    int J = (ins==7); //命令アドレスをジャンプするか


    uint8_t lo=S.D, hi=0, *to=NULL; //Mode選択

    int incX=0;

    if(!J)
        switch (mod){
            #define E(p) (W?0:p)
            case 0: to=E(&T.AC); break; //AC:Accumlator, X:X register, Y:Y register
            case 1: to=E(&T.AC); lo=S.X; break; //lo:RAM下位8bitアドレス(命令の2byte目(Dレジスタ)またはxレジスタの値を格納、hi:RAM上位8bitアドレス 0またはYレジスタの値を格納
            case 2: to=E(&T.AC); hi=S.Y; break;
            case 3: to=E(&T.AC); lo=S.X; hi=S.Y; break;
            case 4: to=&T.X; break;
            case 5: to=&T.Y; break;
            case 6: to=E(&T.OUT); break;
            case 7: to=E(&T.OUT); lo=S.X; hi=S.Y; incX=1; break;
        }

    uint16_t addr = (hi << 8) | lo; //RAM addr、上位8bitがhi,下位8bitがlo

    //どのデータをBUSに流すかを規定
    int B = S.undef;
    switch(bus){
        case 0: B=S.D; break; //DE 即値データをバスに流す
        case 1: if (!W) B = RAM[addr&0x7fff]; break; //OE RAMから読みだしたデータをバスに流す
        case 2: B=S.AC; break; //AE
        case 3: B=IN; break; //IE
    }

    //書き込みフラグが立っていればバスに流れているデータをRAMに書き込む
    if(W) RAM[addr&0x7fff]=B;

    //
    uint8_t ALU;
    switch(ins){
        case 0: ALU = B; break; //LD
        case 1: ALU = S.AC & B; break; //ANDA
        case 2: ALU=S.AC | B; break; //ORA
        case 3: ALU=S.AC^B; break; //XORA
        case 4: ALU=S.AC + B; break; // ADDA
        case 5: ALU = S.AC -B; break; //SUBA
        case 6: ALU = S.AC; break; //ST
        case 7: ALU = -S.AC; break; //JMP
    }

    if (to) *to=ALU; // load value into register
    if (incX) T.X = S.X + 1;

    T.PC = S.PC+1; //Next instruction
    //ジャンプ命令がある場合の次のプログラムカウンタの値を設定
    if(J){ //J = (ins == 7)
        if (mod!=0) { // mod = S.IR>>2 & 7
            int cond = (S.AC>>7) + 2*(S.AC==0);
            if (mod & (1 << cond))
                T.PC = (S.PC &0xff00) | B;
        }else
            T.PC = (S.Y << 8) | B;
    }

    //デバッグ出力
    //cycle
    fprintf(debug_fp, "%03d,",t);
    //ROM VALUE 
    fprintf(debug_fp, "%02x%02x,",T.D,T.IR);
    //IR
    int arr_ir[8];
    eight_bit_str(T.IR, arr_ir);
    for (int i = 7; i >= 0; i--) {
        fprintf(debug_fp, "%d", arr_ir[i]);
    }
    fprintf(debug_fp, ",");

    //D
    int arr_d[8];
    eight_bit_str(T.D, arr_d);
    for (int i = 7; i >= 0; i--) {
        fprintf(debug_fp, "%d", arr_d[i]);
    }
    fprintf(debug_fp, ",");

    //BUS_NUM
    fprintf(debug_fp, "%d,",bus);
    //BUS
    int arr_b[8];
    eight_bit_str(B, arr_b);
    for (int i = 7; i >= 0; i--) {
        fprintf(debug_fp, "%d", arr_b[i]);
    }
    fprintf(debug_fp, ",");

    //ALU
    int arr_alu[8];
    eight_bit_str(ALU, arr_alu);
    for (int i = 7; i >= 0; i--) {
        fprintf(debug_fp, "%d", arr_alu[i]);
    }
    fprintf(debug_fp, ",");

    //AC
    int arr_ac[8];
    eight_bit_str(T.AC, arr_ac);
    for (int i = 7; i >= 0; i--) {
        fprintf(debug_fp, "%d", arr_ac[i]);
    }
    fprintf(debug_fp, ",");

    //RAM ADDRESS
    int arr_addr[16];
    sixteen_bit_str(addr, arr_addr);
    for (int i = 15; i >= 0; i--) {
        fprintf(debug_fp, "%d", arr_addr[i]);
    }
    fprintf(debug_fp, ",");

    //JMP
    fprintf(debug_fp, "%d,",J);

    //ROM_ADDR
    int arr_rom_addr[16];
    sixteen_bit_str(T.PC, arr_rom_addr);
    for (int i = 15; i >= 0; i--) {
        fprintf(debug_fp, "%d", arr_rom_addr[i]);
    }
    fprintf(debug_fp, ",");

    //Xレジスタ
    int arr_x[8];
    eight_bit_str(T.X, arr_x);
    for (int i = 7; i >= 0; i--) {
        fprintf(debug_fp, "%d", arr_x[i]);
    }
    fprintf(debug_fp, ",");

    //Yレジスタ
    int arr_y[8];
    eight_bit_str(T.Y, arr_y);
    for (int i = 7; i >= 0; i--) {
        fprintf(debug_fp, "%d", arr_y[i]);
    }
    fprintf(debug_fp, ",");

    //OUTレジスタ
    int arr_out[8];
    eight_bit_str(T.OUT, arr_out);
    for (int i = 7; i >= 0; i--) {
        fprintf(debug_fp, "%d", arr_out[i]);
    }

    fprintf(debug_fp, "\n");

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

    debug_fp = fopen("debug.csv", "w");
    if (!debug_fp){
        fprintf(stderr,"Error: failed to open debug.csv\n");
        exit(EXIT_FAILURE);
    }

    //output header
    fprintf(debug_fp, "CYCLE,ROM_VAL,IR,D,BUS_NUM,BUS,ALU,AC,RAM_ADDR,JMP,ROM_ADDR_16,X,Y,OUT\n");

    int vgaX=0, vgaY=0;
    for (long long t=-2; t<=500000 ;t++){
        if (t<0) S.PC = 0;

        CpuState T = cpuCycle(S,t);

        int hSync = (T.OUT & 0x40) - (S.OUT & 0x40);
        int vSync = (T.OUT & 0x80) - (S.OUT & 0x80);
        if (vSync < 0) vgaY = -36;
        if (vgaX++ < 200){
            /*
            if (hSync) putchar ('|');
            else if (vgaX == 200) putchar('>');
            else if (-S.OUT & 0x80) putchar('^');
            else putchar(32 + (S.OUT & 63));
            */
        }
        if (hSync > 0){
            //printf("%s line %d xout %02x t %0.3f\n",
            //vgaX!=200 ? "-":"",vgaY, T.AC, t/6.250e+06);
            vgaX = 0;
            vgaY++;
            T.undef = rand() & 0xff;
        }
        S=T;
    }
    fclose(debug_fp);
    return 0;
}

