#romデータをlogisimコンポーネント用のtxtファイルに変換

def convert_gigatron_rom_to_logisim(input_file,output_file):
    with open(input_file, "rb") as f:
        binary_data = f.read()

    with open(output_file, "w") as f:
        # Logisimのヘッダー
        f.write("v2.0 raw\n")
        
        # 2バイトずつ読み込んで16進数として書き出し
        for i in range(0, len(binary_data), 2):
            if i + 1 < len(binary_data):
                ir = binary_data[i]
                d = binary_data[i+1]
                # 16ビットワードとして結合 (IRが上位、Dが下位)
                word = (ir << 8) | d
                f.write(f"{word:04x} ")
            
            # 適宜改行を入れる（読みやすさのため）
            if (i // 2 + 1) % 8 == 0:
                f.write("\n")

    

if __name__ == "__main__":
   convert_gigatron_rom_to_logisim("./gigatron-rom/ROMv6.rom","./rom_logisim.txt") 
    
    