---
layout: post
title:  "Patching a Crackme (oXYgen's PatchMe #1)"
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
change the message that the nag-window is displaying.

Running the program produces a single window with a message for us.

![Running InjectMe.exe]({{ site-url }}/assets/post_images/crackme_original.PNG)

I Opened up the executable in IDA & had a look at the `start` function.

![Disassembly of InjectMe.exe]({{ site-url }}/assets/post_images/idapro_original.PNG)

It's a small function that to my understanding simply pushes some
strings onto the stack & calls the `MessageBoxA`-function in
user32.dll (which opens up the nag-screen)

`Caption` & `Text` references strings that are used when we open up
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

Thanks for reading, hit me up on [twitter](https://twitter.com/jonatanhal) if
that's your thing.


