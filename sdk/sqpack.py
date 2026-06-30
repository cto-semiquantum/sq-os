import sys
import struct
import os

def pack_files(output_path, files):
    """
    Packs a list of files into the SQF\\0 archive format.
    Format of each file block:
      - Magic: 'SQF\\0' (4 bytes)
      - Size: uint32_t (4 bytes)
      - Filename: char[32] (32 bytes, null-padded)
      - Data: <size> bytes
    """
    try:
        with open(output_path, 'wb') as out_f:
            for filepath in files:
                if not os.path.exists(filepath):
                    print(f"Error: File '{filepath}' does not exist.")
                    return False
                
                size = os.path.getsize(filepath)
                # Max size sanity check for hobby OS
                if size > 32768:
                    print(f"Warning: File '{filepath}' is large ({size} bytes). Max recommended size is 32KB.")

                # Convert path to uppercase base name for FAT12 compliance
                basename = os.path.basename(filepath).upper()
                
                # Zero-padded 32-byte filename
                name_bytes = basename.encode('ascii', errors='ignore')
                if len(name_bytes) > 31:
                    name_bytes = name_bytes[:31]
                name_padded = name_bytes + b'\x00' * (32 - len(name_bytes))

                # Write block header
                out_f.write(b'SQF\x00')
                out_f.write(struct.pack('<I', size))
                out_f.write(name_padded)

                # Write block data
                with open(filepath, 'rb') as in_f:
                    out_f.write(in_f.read())
                
                print(f"Packed: {basename} ({size} bytes)")
        
        print(f"Successfully created package: {output_path}")
        return True
    except Exception as e:
        print(f"Exception during packing: {e}")
        return False

def main():
    if len(sys.argv) < 3:
        print("Usage: python sqpack.py <output_package.sqpkg> <file1> [file2] [file3] ...")
        sys.exit(1)
    
    output_pkg = sys.argv[1]
    input_files = sys.argv[2:]
    
    success = pack_files(output_pkg, input_files)
    if not success:
        sys.exit(1)

if __name__ == '__main__':
    main()
