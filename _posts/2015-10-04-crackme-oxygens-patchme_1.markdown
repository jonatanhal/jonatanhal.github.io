---
layout: post
title:  "Crackme - oXYgen's PatchMe #1"
---

I've been meaning to get into reverse engineering for quite some time
now.  And this blog-post marks my first solving of a Crackme found on
[crackmes.de](http://crackmes.de).

Initially I armed myself with a version of IDA-pro for disassembling,
hexplorer for editing the actual executable & I would recommend using
both pieces of software. It's quite a steep learning-curve figuring out
what to do and how it all fits together, but I think I was able
to wrap my head around this *very* basic Crackme.

Here's a [link](http://crackmes.de/users/oxygen/patchme_1) to the
actual Crackme. You need to create an account to download the actual
executable.

{% highlight sh %}
$ md5sum InjectMe.exe 
96bf3074d7f65aab8660688ce50084a4  InjectMe.exe
{% endhighlight %}

The executable (InjectMe.exe) is provided along with a read-me:

> This is a PatchMe in full ASM (My First)
>
> You need to add a Nag Screen and write your name in the Main Window (After your nag screen)
> You can use what you want, you can do what you want. You just need to do these 2 things
>
> Have Fun Reversing ! ;D
>
> OXY <3

So if I'm reading this right, I need to patch the executable and thus
change the message that the nag-window is displaying. I also need to
add another nag-screen before the already existing nag-screen pops up.

* * *

## 1. **Changing the main nag screen**

I Opened up the executable in IDA & had a look at the `start` subroutine.

![Disassembly of InjectMe.exe]({{ site-url }}/assets/post_images/idapro_orginal.PNG)

It's a small subroutine that to my understanding simply pushes some
strings onto the stack & calls the `MessageBoxA`-function in
user32.dll (which opens up the nag-screen)

`Caption` & `Text` references strings that are used by windows when we open up
the executable.

![Strings abound!]({{ site-url }}/assets/post_images/idapro_strings.PNG)

We can find out the actual position of the text in the executable by
clicking on the line in IDA. In the lower-left of our
Disassembly-window we see the location (highlighted in yellow)

![The string location]({{ site-url }}/assets/post_images/idapro_stringlocation.PNG)

If we also find out where the `push offset Text` op-code is in the executable, we can simply
change the reference to whatever we want :)

Clicking said instruction in IDA reveals it's location (marked in red.)

![The instruction location]({{ site-url }}/assets/post_images/idapro_instructionlocation.PNG)

We put the pieces together by patching the binary using hexplorer. We
start by going to the location (hotkey <kbd>F5</kbd>) `0x407` & change
the operand to `0x403041` (which points to `aGoodBoyXxxxxxx` we saw in IDA)

*Note: The machine-code is small-endian, so `00 30 40` is actually `40 30 00`.*

![Patching the executable]({{ site-url }}/assets/post_images/hexplorer_codechange.PNG)

*Hotkey Pro-tip: <kbd>F7</kbd> increments the hexadecimal value, and
<kbd>F8</kbd> decrements the hexadecimal value*

With my patch in place, I just changed the string located at position
`0x841` of the executable to my glorious name & rejoiced as I ran the
modified version of the binary.

![Patched version]({{ site-url }}/assets/post_images/crackme_patched.PNG)

* * * 

## 2. **Adding our own nag-screen**

How do we add our own nag-screen?

*Well*, it shouldn't be too hard considering the 5 lines of disassembly in
the start-subroutine to produce a single nag-screen. All we need to do
is to inject our own machine-code into the executable.

When I've worked with hexplorer previously, It didnt seem possible
to simply add data, since all that seems to happend that i overwrite data.

![A alot of zeros]({{ site-url }}/assets/post_images/before_start.PNG)

But there seems to be a lof of space before and after the actual
machine-code. This empty space is referred to as a code-caves. So
all we need to do is to overwrite some of the zeroes with a slightly
modified copy of the existing nag-screen in order to create our own?

I actually tried to overwrite some of the leading zeroes with a copy of the
machine-code, when it didn't work I figured the PE-header was not pointing
to my leading instructions, and thus they were completely ignored.

When I changed the header to point to my instructions instead,
Windows gave me the finger & complained that my glorious exe was not valid.
Maybe that's because I pointed the header in the wrong direction, who knows
`¯\_(ツ)_/¯`

Just before getting on the train to *frowntown*, I googled the problem
I was faced with, which led me to a guide written by **Iman Karim** which
made things very clear & easy.

I need to change the leading first instruction - `push 0` with a `jmp`
instruction that points to injected code which includes the
overwritten instruction & jump back to allow the program to continue execution
as if nothing happened.

In pseudo-assembly, this would translate to something like the
following:

{% highlight nasm %}
jmp ourCode ; !! Overwritten instruction
push mainCaption ; offset 0x100
push mainText
push 0
call MessageBox
; omitted remaining instructions

ourCode:
 push 0          ; First parameter (handle of owner window)
 push ourCaption ; Address of our caption
 push ourText    ; Address of our body-text
 push 0          ; Last parameter (style of the message box)
 call MessageBox ; Hey bill gates, show this window
 ; Now we just need to include the overwritten
 ; instruction & jump back to the main program.
 push 0
 jmp 0x100
{% endhighlight %}

This is easy if we use ollydb, where we can simply select
a code-cave & chose to write ascii or assembly-instructions.
Provided we wrote in the correct offsets, it's just a matter
of applying all changes to the executable & execute that bad-boy.

![Injection via OllyDB]({{ site-url }}/assets/post_images/ollydb_injection.PNG)

Here we have my very own nag-screen.

![Naggers]({{ site-url }}/assets/post_images/my_nagscreen.PNG)

I really recommend reading the guide (link is provided below), since
it clearly explains the steps in injecting code in a executable (in
contrast to my ramblings)

### **Shoutouts**

**[Guide for injecting code into a executable](http://www2.inf.fh-bonn-rhein-sieg.de/~ikarim2s/how2injectcode/code_inject.html)**

**[Corkami-projects 101-posters](https://github.com/corkami)**



* * * 

Thanks for reading, hit me up on [twitter](https://twitter.com/jonatanhal) if
that's your thing.


