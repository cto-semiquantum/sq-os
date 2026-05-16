def test_loop():
    memory = bytearray(1024)
    commands = b'help\0clear\0about\0reboot\0version\0whoami\0\0'
    memory[0:len(commands)] = commands
    
    input_str = b'whoami\0'
    memory[512:512+len(input_str)] = input_str
    
    di = 0
    bx = 0
    while memory[di] != 0:
        si = 512
        match = True
        
        while True:
            al = memory[si]
            si += 1
            ah = memory[di]
            di += 1
            if al != ah:
                match = False
                break
            if al == 0:
                break
                
        if match:
            print(f"Matched at index {bx//2}")
            return
        
        if memory[di-1] != 0:
            while True:
                # INC DI bug simulation: ZF is overwritten by INC!
                is_zero = (memory[di] == 0)
                di += 1
                # jne relies on ZF. Since INC DI clears ZF if result is non-zero,
                # jne will jump as long as DI != 0!
                zf = (di == 0) # INC sets ZF only if result is 0 (wraparound)
                if zf:
                    break # jne fails
        bx += 2
        
    print("Not found")

test_loop()
