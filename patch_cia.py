import sys

with open("SnakeEngine.cia", "rb") as f:
    data = bytearray(f.read())

idx = data.find(b'NCCH')
if idx != -1:
    print(f"NCCH found at {hex(idx)}")
    
    # Flag1: L2 Cache (bit 0) and 804MHz CPU (bit 1) -> 0x03
    flag1_offset = idx + 0x30C
    # Flag2: New3DS System Mode (124MB is 1) -> 0x01
    flag2_offset = idx + 0x30D
    
    old_flag1 = data[flag1_offset]
    old_flag2 = data[flag2_offset]
    print(f"Old Flag1: {hex(old_flag1)}, Old Flag2: {hex(old_flag2)}")
    
    data[flag1_offset] = 0x03
    data[flag2_offset] = 0x01
    
    with open("SnakeEngine.cia", "wb") as f:
        f.write(data)
    print("CIA RAM & CPU Flags Patched Successfully!")
else:
    print("NCCH not found")
