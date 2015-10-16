---
layout: post
title:  "Crackme - QPatchmi No2"
---

Hello again, today we're gonna break another crackme, this time we're
gonna break the [QPatchmi No2](http://users/qhqcrker/qpatchmi_no2)
which can be downloaded (assuming you've registred an account on
[crackmes.de](http://crackmes.de))

Inside the downloaded zip-file is a executable, for those of you following
along at home, here's a md5sum provided.

{% highlight sh %}
 $ md5sum QHQPatchmeNo1.exe 
49f172d35190904698cb8bf4023d5a96  QHQPatchmeNo1.exe
{% endhighlight %}

When first starting up the executable, we're faced with our instructions.

> Hello newbiez world. Try with me!
>
> Rulez :
>
> 1. Patch this nag.
> 2. Patch the BADBOY static
> 3. Make Exit button Visible.
>
> Hope you fun with me...

*Tools used: Hexplorer & IDA*

## 1. **We dont take kindly to nag-screens 'round here.**

Having a look at the start subroutine, among the disassembled
instructions, a `call sub_401519` is present. I had a look at
`sub_401519` and seeing `hDialogTemplate` in the disassembly,
I assumed that `sub_401519` is what makes
the nag-screen (AKA Dialog box) appear.

*So how do we remove it?*

We can find out where the instruction (`call sub_401519`) is
in the binary by clicking the instruction in IDA & looking at
the lower left-hand side of the screen, where we see
three kinds of information.

![Instruction location]({{ site-url }}/assets/post_images/instruction_location_ida.PNG)

Where `0000041C` is the current offset in the input-file.

Opening up the executable in hexplorer, and jumping to the
address `0x41c`, we're in the right location; all need to do now
is actually write our own instructions into the executable that will
further our goal.

But what instruction do we want to write over the original instruction?

Any old instruction would do I guess, but i chose the `nop`-instruction,
which does nothing. `nop` is represented as `0x90`.

![Patched nag-screen]({{ site-url }}/assets/post_images/patched_nag_screen.PNG)

Note the `90 90` marked in blue, it's my patch :)

*Pro-Tip: The hotkey for jumping to an address is  <kbd>F5</kbd> in hexplorer*

Now when I startup the executable, we're faced with only the main
screen, no nag-screen!

To celebrate & rest our eyes from assembly and hex, while also
depicting my own reaction after doing this; here's a picture of Slavoj Zizek.

![Happy Slavoj]({{ site-url }}/assets/post_images/happy_zizek.jpg)

## 2. **Patching the BADBOY-static.**

Similiarly to the previous step where we patched a instruction, next
we're gonna patch some text in the binary.

This step is just a matter of locating the string we want to change
in hexplorer & overwriting the text.

Finding the text is just a matter of pressing <kbd>ctrl-F</kbd> & searching
for the text `--> Patch this message to good message <--`.

![Shameless hacking]({{ site-url }}/assets/post_images/patched_badboy.PNG)

But wait, maybe the author of this crackme actually means we should
reference another piece of text found elsewhere in the crackme?

This struck me as I wandered through the executable in IDA. Because in
addition to there being `aPatchThisMessa` (which contains the `-->
Patch this message to good message <--` in the .data section, we can
also see `aCongratulation` inwhich the `CONGRATULATION, YOU'RE GOOD
BOY` string lies.

The disassembled `push offset aPatchThisMessa` instruction is found
at `0x6af` & in hexadecimal it looks something like this:

`68 97 31 40`

Patching it is just a matter of changing the operands to point to
the `GOOD BOY`-string (`0x40310e`)

But when I patched it in hexplorer (changing `68 97 31 40` to `68 0e 31 40`), The program would not run
like it used to, It would just exit (or crash?) after showing the nag-screen.

**WTF?**

## 3. **Making the exit-button clickable.**

Looking at the disassembly in IDA & the `sub_40104F` subroutine,
`loc_4013e7` references the `Exit` text found in the window.

![IDA loc_4013e7]({{ site-url }}/assets/post_images/ida_loc_4013e7.PNG)

*So what the hell does all this assembly do?*

**Good question!** - I have no idea, I can't make sense of assembly
(yet)... I just take advantage of IDA giving me hints & try stuff
until I either loose focus or get bored.

Well, We can hopefully figure out what's needed for a button
to become clickable if we compare the instructions of our patched-out
dialog-box with the one we're trying to <del>break</del> fix.

