f_name = raw_input("filename (warning - truncates file): ")
b_count = int(raw_input("byte count: "))
f = open(f_name, "wb")

f.write(chr(0x90) * b_count)

f.close()
