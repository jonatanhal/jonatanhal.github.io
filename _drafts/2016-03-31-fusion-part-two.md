---

layout: post
title:  "Fusion Exploit Exercises: Part Two"
author: jonatanhal

---


# [level04](https://exploit-exercises.com/fusion/level04/)

>About 
>
>Level04 introduces timing attacks, position independent
>executables (PIE), and stack smashing protection (SSP). Partial
>overwrites ahoy!

There's some hoops that we have to jump through in this level, we need
to guess a randomly generated password used when authenticating to the
http-server, and we also need to adjust our exploit to make it
sidestep the issue of [SSP](http://wiki.osdev.org/Stack_Smashing_Protector)
and PIE.

### SSP in a nutshell

SSP is typically implemented as a saved 64/32 bit value on the stack,
located next to the saved return address. The saved value can be
predefined or generated randomly; As far as I know, it is implemented
as a randomly generated value by glibc/gcc.

When overflowing a buffer in a attempt to overwrite the saved
return-address, the saved value (referred to as a stack-canary) will
also overwritten. Before the function returns, the canary is checked.

If a canary-change is detected, proper action can be taken.

### No SSP here, SSP everywhere else.

There does not seem to be a stack-canary in the `base64_decoder`
function, as there are no calls to `__stack_chk_fail_local`. As opposed to
the function `validate_credentials`, which do include calls to
the aforementioned function.

The details-variable in `validate_credentials`, is likely is the 
target (since it's the only stack-allocated buffer which size
is below 10k.

I'm guessing that the `base64_decoder` function somehow got excluded from
the SSP-treatment by the compiler. So, as long as we don't overflow the 
stack-canary in a function calling `base64_decoder`, we should be ok.

### Authenticating :~)

Parts of the code are testing if the connection being made to the
http-server knows about a secret password that's being generated once
the daemon is started. The password does not change between
connections, So we dont have to have god-luck when trying to guess it.

Having a look at the code, we contemplate over the following section

{% highlight c %}
// ---- Snippet from validate_credentials ----

// anti bruteforce mechanism. good luck ;>
  
tv.tv_sec = 0;
tv.tv_usec = 2500 * bytes_wrong;

select(0, NULL, NULL, NULL, &tv);
{% endhighlight %}

Before reaching this point in the code, a loop is iterating over the 
base64-encoded password that we're sending, any byte that does not
match the randomly generated password increments the `bytes_wrong`
variable.

So when the code above is ran, if we're completely wrong (none of
the bytes we're guessing are correct) then we should expect
the function to wait 40000 microseconds before doing anything
else.

You get the point, if our guess only has 15 wrong bytes - then the
function would wait 37500 microseconds before continuing execution.

## Good stuff

* [Exploiting Timing Attacks in Widespread Systems](https://www.youtube.com/watch?v=idjDiBtu93Y&list=PL2B7CF99109106D36)

* [Time is on my Side - Exploiting Timing Side Channel Vulnerabilities on the Web](https://www.youtube.com/watch?v=OsbQhr9ZmPY)

* * * 

# [level13](https://exploit-exercises.com/fusion/level13/)

>About
>
>There is no description available for this level. Investigate the
>binary at /opt/fusion/bin/level13!

Cool! First level without any source-code :D

It's apparently got no security options enabled, which is neat.

The level13 binary contains stabs debugging information, one way of
reading that information is by using objdump with the `-g` switch.

The information helps us pinpoint the different types, sizes and
variable names.

{% highlight c %}
 level13/level13.c:
int process ()
{ /* 0x1dc0 */
  { /* 0x1dc0 */
    char again[64]:uint32 /* 0xffffffa4 */;
    char result[2048]:uint32 /* 0xfffff6e4 */;
    char *wh00p /* 0xfffff6e0 */;
    char email[64]:uint32 /* 0xffffff64 */;
    char password[64]:uint32 /* 0xffffff24 */;
    char username[64]:uint32 /* 0xfffffee4 */;
    // snip-snip
  } /* 0x1f60 */
} /* 0x1f60 */
{% endhighlight %}

Here we see the function prologue, along with three sets of the `rep
stosd` instruction, which is used to zero out the 64 bytes in three
buffers.

Note the call to `__i686.get_pc_thunk.bx` which places the address of
the instruction following the call, into ebx.

![radare2 view of process 01]({{ site-url }}/assets/post_images/fusion_level13_process01.png)

Such a call would indicate that the binary is composed of
position-independent code, where global or static variables would be
referenced by their offset to the saved position, in this case
`0x1dd1`. `0x1d83` is then added to ebx, resulting in it containing
`0x3b54`.

For instance, in a call to the get_string-function is constructed like
this:

![radare2 view of process 02]({{ site-url }}/assets/post_images/fusion_level13_process02.png)

We can find have a look at the `get_string` function definition by
searching for it in the stabs-information dumped by objdump.

{% highlight c %}
void get_string (char *prompt /* 0x8 */, char *buffer /* 0xc */, int buffer_len /* 0x10 */)
{% endhighlight %}

With this knowledge amassed, we can assume that the local variable
`local_9ch`, is the local email-variable.

That's all fine and dandy, and our disassembly is more readable (for
those of you following along at home, renaming local variables in r2
can be achieved via the `afvn [old_name] [new_name]` command.)

### Alien Techonolgy

In the beginning of the function, the local variable `wh00p` is set with
the following instruction-sequence.

{% highlight nasm %}
mov eax, dword gs:[0x14]
mov dword [ebp-wh00p], eax
{% endhighlight %}

While debugging the program, the code above resulting in `wh00p` being
`0xf2ca3f00`, wat. When restarted, the stored value is `0x1f3aba00`...
wat.

:TODO: Find out what the code @0x1dd7 does.

Somehow, `gs:0x14` == `0xf2ca3f00`. wtf? :D

* `process` returns into main+49


* * * 






