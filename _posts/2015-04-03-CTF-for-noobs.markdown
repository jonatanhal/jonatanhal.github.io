---
layout: post
title:  "Capturing infosec-flags"
---
#Overview
I tried to solve some of the Capture-the-flag challenges over at ctf.infosecinstitute.com
Here's just me typing down things as i go through the challenges.

#Challenge 1
I started writing this post after made *some* "progress" on the challenge.
![Challenge 1 screen]({{ site-url }}/assets/infosec_ctf_ch01.jpg)

A picture of Yoda & the text "May the source be with you!", great!
So, I started by looking at the source of the HTML, looking for any
script or weird HTML give me some clues on the challenge. There's nothing
in the HTML or JavaScript that does anything out of the ordinary, so after
fiddling with the `src="..."` attribute on the image, I went hunting elsewhere.
Opening up the devtools-thingy in Firefox & looking at the response-headers of
the connections to the website, I saw the ETAG Header for the first time.
I spent some minutes getting familiar with ETAGs & came to the conclusion
that it's probably not to do with the challenge.

With that behind me, I returned to the HTML-source & glanced at the top of the source.
Staring at me, in it's humble representation, was what I believe to be the first flag.

`<!-- infosec_flagis_welcome -->`

### Conclusion, Challenge 1:
+ Flag: welcome
+ Difficulty: Eyes

* * *

#Challenge 2
Here we go, Challenge 2, I'm excited :D
![Challenge 2 screen]({{ site-url }}/assets/infosec_ctf_ch02.jpg)

OK, text is `< It seems like the image is broken..Can you check the File? >`,
located above it is a broken `img`-tag. Sounds reasonable, let's take a look.

Using my super-secret, cross-compiled version of Firefox; I was able to
again press F12 (Please don't tell anyone) & Gain access to the HTML.

~~~ html
<img src="img/leveltwo.jpeg">
<br>
<br>
<p>
	<button class="btn btn-large btn-primary" type="button">
		It seems like the image is broken..Can you check the file?
	</button>
</p>
<br>
<br>
<br>
<p style="font-size:.9em;font-weight:normal;">Bounty: $20</p>
~~~

The image is indeed broken, but something's not right, because
the servers returned a `200 OK` when we requested the file. So the
file's not missing, it's just not a image. There's something fishy going on here.
Let's fire up a shell & do some poking on the file in question.

~~~
$ curl -O http://ctf.infosecinstitute.com/img/leveltwo.jpeg
...
$ ls -l leveltwo.jpeg 
-rw-r--r--   1 jh             jh             45 2015-04-03 16:30 leveltwo.jpeg
$ file leveltwo.jpeg 
leveltwo.jpeg: ASCII text
$ # :^)
$ cat leveltwo.jpeg 
aW5mb3NlY19mbGFnaXNfd2VhcmVqdXN0c3RhcnRpbmc=
~~~

Looks like base64 encoding, let's try and decode it using python.
Starting by opening up the file in python & reading the contents
of said file into a variable.
Stripping out the files newline character as we go along.

~~~
$ python
Python 3.4.3 (default, Mar 25 2015, 17:13:50) 
[GCC 4.9.2 20150304 (prerelease)] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> with open("leveltwo.jpeg", 'r') as f: line = f.read()
... 
>>> line
'aW5mb3NlY19mbGFnaXNfd2VhcmVqdXN0c3RhcnRpbmc=\n'
>>> line = line[:-1] # remove the trailing newline-character
...
~~~
We import pythons base64 module, which will allow us to do pretty much anything
concerning base64 encoding, decoding & whatnot.
One thing I LOVE about python is the built in `dir`-function. `dir` is simply a
function that takes any parameter & returns a list of attributes or methods
that the passed parameter has associated with it.

Finding out what one can do with a variable or object makes it easy to
test, break & find stuff, because one does not have to wade through stack-overflow,
docs or man-pages looking for that one or two things You want to do with a library.

But that's more about python, we're doing infosec CTF-stuff, lets import the
library we want & see if we can find it's relevant methods.

~~~
>>> import base64
>>> dir(base64)
[ ... 'decode', 'decodebytes', 'decodestring', 'encode', 'encodebytes', 'encodestring', ...] 
>>> base64.decodestring(line)
~~~

At this time, the base64-library gods is angered that we passed a string into
it's function.

~~~
File "/usr/lib/python3.4/base64.py", line 522, in _input_type_check
    raise TypeError(msg) from err
TypeError: expected bytes-like object, not str
# :(
~~~

We acknowledge that the gods are helpful gods & their response initially
perceived as anger is only an attempt to guide us mere mortals along the way.
We please the gods by passing a byte-representation of`line` into the function.
We do this by using the built in `bytes` function of python, passing along what
encoding the string is.

~~~
>>> base64.decodestring(bytes(line, 'utf8'))
b'infosec_flagis_wearejuststarting'
~~~

Boom goes the dynamite.

*Authors Note: My solution could maybe take place purely in
the browser, however I am more familiar with using Python so,
that's what i felt comfortable using.* 

###Conclusion, challenge 2.
+ Flag: wearejuststarting
+ Difficulty: Computer

* * *

#Challenge 3.

![Challenge 3 screen]({{ site-url }}/assets/infosec_ctf_ch03.jpg)

We are presented with a Qr-code & progress-bar, stuck at 90%.

Decoding the Qr-code is as easy as taking out your phone & run
your favorite Qr-code app to decode it.
(I used [Barcode Scanner](https://f-droid.org/repository/browse/?fdfilter=barcode&fdid=com.google.zxing.client.android))

When the scanning completes, I am presented the encoded text.

`.. -. ..-. --- ... . -.-. ..-. .-.. .- --. .. ... -- --- .-. ... .. -. --.`

Ohai Morse-code.

Turning the Morse-code into their letter-counterpart produces this:

`I N F O S E C F L A G I S M O R S I N G`

###Conclusion, Challenge 3.
+ Flag: morsing
+ Difficulty: Smart-phone

* * * 

# Challenge 4.

![Challenge 4 screen]({{ site-url }}/assets/infosec_ctf_ch04.jpg)

Oh look, it's Cookie Monster from Sesame street, with his lil' pal
whom I haven't seen before. This makes me suspect that this challenge
has to do with cookies, and I'm not talking about the kind you
eat too many of, experience a mild sugar-crash & have to go lie down
for a while :^)

If we use the pointer to hover over the image, we get an alert, pleading
for us to stop poking the image, this is merely a distraction setup by
the CTF-gods & I refuse to have it tell me what to do. After several hours
of poking the image, I finally give in & gave in to the power of JavaScript.

Defeated, somewhat sweaty & ashamed of myself, I began
looking for the flag.

Using <s>my F12-exploit</s> developer-tools again in Firefox
, I can click my way to the network-tab & with `levelfour.php`
selected, inspect the cookies associated with the request.
![Challenge 4 developer-tools cookie inspection]({{ site-url }}/assets/infosec_ctf_ch04_cookies.jpg)

Ohai cookie.

`vasbfrp_syntvf_jrybirpbbxvrf`

Looking at it, the general form seems familiar, doesn't it? It
kinda looks like the previous flag texts. But hey, Lets do some <s>looking at it</s> **visual analysis**

`vasbfrp_syntvf_jrybirpbbxvrf`  
`infosec_flagis_wearejuststarting`  
`infosec_flagis_welcome`

So my guess at the moment is that this is just single-character
substitution, because the characters & substituted characters show
up where they are expected to show up. That's probably a terrible
explanation so let me try and show you what I mean.

**Single-character Substitution**

> In cryptography, a substitution cipher is a method of encoding by
> which units of plain-text are replaced with cipher-text, according to
> a regular system; the "units" may be single letters (the most common),
> pairs of letters, triplets of letters, mixtures of the above, and
> so forth. The receiver deciphers the text by performing an inverse substitution.

[Wikipedia Article - Substitution cipher](https://en.wikipedia.org/wiki/Substitution_cipher)

So we can create a substitution-alphabet, explaining what character maps
to it's substitution. And if the message-receiver also knows this
substitution-alphabet, he or she could reverse the substitution & read the message.
Sounds reasonable!

For those of you playing along at home, We will now try and figure out
what the Cookies substitution-alphabet is!

Looking at what our guesses about the ciphers different characters are,
we can begin to fill-in the question-marks in our table :D

`vasbfrp_syntvf_jrybirpbbxvrf`  
`infosec_flagis_*************`

I is probably substituted with V, N for A & so on. Expanding upon that,
we can guess the rest of the message, or at least parts of it.

`vasbfrp_syntvf_jrybirpbbxvrf`  
`infosec_flagis_*elo*ecoo*ies`

Right now, we do not know what characters J, I & X are substitutions for,
but We do know that the substitutions are one of the characters still unmapped
in our substitution-map.

We can make guesses to figure out the remaining three characters of
the cipher, If the substitution makes sense, we can go ahead and add
it to our substitution-map. It's safe to assume that any one character
can map to only one substitution.

In the end, this is the original message.

`infosec_flagis_welovecookies`

### Conclusion, Challenge 4.
+ Flag: welovecookies
+ Difficulty: 10/10, would scratch my head again.

* * *

# Challenge 5

![Challenge 5 screen]({{ site-url }}/assets/infosec_ctf_ch05.jpg)

Upon entering this challenge, we are greeted by seemingly endless
`alert`s, calling us hackers. When the alerts stop, we are greeted by
the screen.  The image is a pretty dank meme, the Alien-guy from the
history channel & the caption
`I'M NOT SAYING IT WAS ALIENS... BUT IT WAS ALIENS`.

So where does this leave us, What's going on here, Aliens, what?

By inspecting the HTML, we can see the evil code summoned those
alerts, but nothing else.

A difference from the other pages is that the challenge `div` has an
additional class-value (`hero-unit`). Again, I doubt that this has any
significance.

Thinking about that dank meme.
So Alien-guy is telling us that even though he's not saying that it
was aliens, it was (contrary to his previous statement), in fact aliens.

I don't get it.

### Conclusion, Challenge 5.
+ Flag: `null`
+ Difficulty: Windows 8.1

* * *

I'll revisit the rest of the challenges another time. Thanks for reading


