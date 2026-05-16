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
                di += 1
                if memory[di-1] == 0:
                    break
        bx += 2
        
    print("Not found")

test_loop()
