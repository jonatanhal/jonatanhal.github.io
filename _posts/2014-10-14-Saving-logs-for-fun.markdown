---
layout: post
title:  "Trying to make some sense out of UFW logs"
---

**Authors note:** Looking back on this post, it's pretty crap; but seeing as I was just starting out with programming in python & stuff, I was eager to do something for my blog. I know it's crap & I may rewrite this post entirely sometime in the future.

**Have you ever typed `dmesg` in your terminal of choice & scratched your head at what all these logs actually mean?**

In my case, i had a similar train of thought when i installed [UFW][ufw] & made it my system firewall.
I typed in dmesg, and saw walls of logging scrolling by, put there by UFW to let me know something about what it did.
Now before we try to understand the logs that [UFW][ufw] spits into the kernel ring-buffer, I would like to emphasize to correct me if I'm wrong, [tweet me][contact].

With that said, lets go.

## Getting some UFW logs to play with
In the case of UFW, you can get your very own logs by changing the `LOGLEVEL`-setting in `/etc/ufw/ufw.conf` to `LOW`, or if you want more logs, crank up the loglevel to medium or high. Take a look in `man ufw` before, because apparently `MEDIUM` & `HIGH` produces a lot of logging.  
At this point you can just run `dmesg -C -T | grep UFW >> /path/to/saved/log` & bob's your uncle! The mac-address is also stored within the logs, but you can replace the macs from your logs by running `dmesg -T | sed 's/MAC\=\([0-9a-f]\+\|\:\)\+/MAC=\[REDACTED\]/'` & pipe that to whatever file you like.

Note that `dmesg -C` will clear the ring-buffer on GNU/Linux operating-systems, meaning that you will no longer be able to access the previous messages.
`-C` is used here to be sure we don't get any duplicates in our logs.

If you don't have any UFW-logs to play with you can grab an example [here](https://github.com/jonatanhal/logparser/blob/master/example_log)

Alot of other programs available on unix produce logs & they can more often than not be found within the `/var/log/` directory.

##Taking a closer look at UFW logs

Knowing something about the data-set you are working with might help you formulate or overcome a problem, if you should change your schema to accommodate for other types & store some value in a Boolean instead of a string based on what you observed.  
How you would get to know something about a different data-set would be beyond the scope of this post, but suffice to say that you would need the help of your search-engine of choice.

So we are, just for a moment taking a brief tour of the UFW-logs to see whats up.


If we take a look at a typical log-message for a blocked TCP-packet when using `dmesg -T` _(`-T` for human-readable time-stamps)_ we see all sorts of information, data about the block, warn or any of the flags that our firewall might want to tell us.
Here's an example log-message with MAC-address redacted, because privacy. If you ever had a logging firewall installed, this might be familiar to you.

`[Wed Jul 30 22:28:27 2014] [UFW BLOCK] IN=wlp3s0 OUT= MAC=[REDACTED] SRC=173.194.xxx.xxx DST=192.168.1.75 LEN=40 TOS=0x00 PREC=0x00 TTL=46 ID=27800 PROTO=TCP SPT=443 DPT=60979 WINDOW=0 RES=0x00 RST URGP=0`

If we take a look at the individual pieces of information it starts out pretty basic with `[Wed Jul 30 22:28:27 2014] [UFW BLOCK]...` telling us that our firewall successfully blocked a packet at the provided time-stamp.
And if we leave out the `-T`-stamp when calling `dmesg` we would get the amount of seconds since our operating started. For example
`[  991.460987] [UFW BLOCK]...`

If we take a look at the properties or anatomy of a TCP-packet we see that a lot of the information in our logs is referenced there, so the logs might not be as daunting as they might have appeared.

![Anatomy of TCP-headers]({{ site-url }}/assets/post_images/tcp-headers.jpg)  
_Image courtesy of [frozentux][frozentux]_

* * * 

| Log-data            | Brief explanation |
|+--------------------|+------------------|
| `[$TIMESTAMP] [UFW BLOCK] `  | Date & action taken, in this case `BLOCK`                                                               |
| `IN=wlp3s0`         | What interface received the packet                                                                               |
| `OUT=`              | What interface was gonna pass on the packet, in this case `OUT=` is empty because we don't NAT on our interfaces |
| `MAC=[REDACTED]`    | contains the MAC-address of the interface the packet was addressed for.          |
| `SRC=173.194.xxx.xxx` | the source IP-address for the packet													                         |
| `DST=192.168.1.75`  | the destination IP-address for the packet												                         |
| `LEN=40`            | the length of the packet																	                     |
| `TOS=0x00`          | type of service, can be used for packet-filtering									                             |
| `PREC=0x00`         | [Help me fill this out][contact]																			     |
| `TTL=46`            | Time to live _AKA. hoplimit_, used in limiting the lifespan of a packet.                                         |
| `ID=27800`          | [Help me fill this out][contact]																			     |
| `PROTO=TCP`         | Protocol, in this case TCP																		                 |
| `SPT=443`           | Source port, Port-number that "hits" your machine									                             |
| `DPT=60979`         | Destination port, Port-number used by your machine to route traffic.					                         |
| `WINDOW=0`          | the number of bytes that the sender is willing to receive										                 |
| `RES=0x00`          | Reserved																					                     |
| `RST`               | Reset flag, other flags include `ACK`, `SYN`, etc. 																 |
| `URGP=0`            | Urgent pointer, _if the URG flag is set, then this 16-bit field is an offset from the sequence number indicating the last urgent data byte_[^3] |
|==========+===================|

**DISCLAIMER**

I have a _very basic_ understanding of the TCP-stack, so some of these things might seem obvious if you are familiar with the protocol.
Some people say that the wisest people are those that admit that they know nothing. I must emphasize that i both know nothing & that I'm not wise to begin with.
  
* * *

## Separate 

If we use python we can quite easily use these as fodder in our database-worker or generate .CSV-files but we need to parse the pieces of information first.
I used python for doing this but hopefully the code should be easily understandable so as to make porting trivial.

{% highlight python %}

def extract(line): # our function takes a single log-line
        
    # init our document
    document = dict()

    # We split the line in two 
    date = line.split('[UFW BLOCK]')[0]
    # data contains the rest of the log
    data = line.split('[UFW BLOCK]')[1]

    document.update({'date': date})

    for d in data.split():

        # Skip data regarding in and out-devices
        if 'IN' in d or 'OUT' in d:
            continue
            
        # Skip data regarding machine MAC & destination IP
        if 'MAC' in d or 'DST' in d:
            continue

        # If there is no = in d, mark flag as True
        # (for example RST-flag in tcp)
        if '=' not in d:
            document.update({d : True})
            continue

        key = d.split('=')[0]
        val = d.split('=')[1]
        document.update({key : val})
{% endhighlight %}

This function would generate dictionaries that we could do fancy stuff with in python.
A somewhat more elaborated implementation which puts the logs into a CouchDB-database can be found in my [logparser-project][logparser]

## What now?

We now know a little bit about the data that landed in our logs & we briefly covered how to parse some of these using python.  
So what can we do with this data?

+ We can parse the data and shove it in a database[^4]
+ We could run some statistical analysis on the logs to find out things like what IP-address is most commonly blocked & what port-numbers are most commonly blocked & so on.
+ Generate nice graphs using our favorite programming-language.
+ Run the data through [processing](http://processing.org) to do something cool. For example, What are some of the similarities between [RGBA](https://en.wikipedia.org/wiki/RGBA_color_space) & IP-addresses? 

I might do another part to this post, where i try to do just that, but for now I need to take a break from writing.

Thanks for reading.

**Updated 2015-05-10: Reformatting & structural changes to the post.**

[ufw]: https://launchpad.net/ufw "Uncomplicated firewall"
[wiki-dmesg]: https://en.wikipedia.org/wiki/Dmesg "Display message"
[frozentux]: https://www.frozentux.net/iptables-tutorial/iptables-tutorial.html
