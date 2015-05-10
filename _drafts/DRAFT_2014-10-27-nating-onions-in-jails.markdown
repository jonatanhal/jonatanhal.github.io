---
layout: post
title:  "NAT'ing Onions in Jails"
---

https://www.freebsd.org/doc/handbook/jails-ezjail.html
https://www.secure-computing.net/wiki/index.php/FreeBSD_jails_with_ezjail
http://www.bsdnow.tv/tutorials/tor
http://www.bsdnow.tv/tutorials/jails

> #TL;DR
> A guide for setting up a tor-relay within jails on FreeBSD

# Googling for solutions to my problems piss me off.
Googling sometimes sucks when you are dealing with a semi-obscure problem. This is
brought on by the fact that I try to do something that there is neither a single
solution for, nor a nontrivial task for a noob like me.

Because when I try to setup my lil' NAS to NAT on FreeBSD jails, I stumble
across a sea of not-so-contextually-explained solutions that might work
for the person who wrote the brief guide or pointer in the first place.

## It should be easy... and it probably is.
But when you have to shard four or five different "help me WTF do i do"-posts for how
to achieve something that you dont know how to do, you're in rough shape.

The problem is that it's probably easy, but I keep thinking that it's easier to find
someone else's solution to their problem. Which, hopefully resemble my own.

So i google & google, try a solution and end up with a either a broken .conf-file,
three different & dangling options set on by different motives & atleast one never-
again touched file or line in some file.

### In my case, it's not complicated.
The goal i was trying to achieve was to setup a tor-relay behind a jail on FreeBSD,
where the service would be isolated from the rest of my system. Using
[PF](http://www.openbsd.dk/faq/pf/) to do natting & whatnot.

And, as I've stated above, i found more solutions than i bargained for, many of which
only portrayed themselves as solutions, allthewhile being nothing more than being
a source of headscratching & confusion.

**So I'll write my own damn guide, because I'm tired of realizing that someone else's
solution to my problem probably wont work.**

![Fuck](http://imgs.xkcd.com/comics/standards.png)

* * *

# Instructions unclear, [REDACTED] stuck in [REDACTED]
[Tor](https://torproject.org) is a wonderful thing, which provides some privacy
those who feel they need it.

Running a relay is a good thing as it provides the tor network with bandwidth. And if you can't
run a relay, but care about privacy, consider [donating to those who do](https://oniontip.com/)
. Some also argue that running a relay is beneficial for your own anonymity on tor, since
torified traffic will flow more regurlarly as a part of you running a relay.

If you found my guide helpful & feel like giving me a 'lil something.
Don't, give it to organisations like [EFF](https://supporters.eff.org/donate),
[Noisebridge](https://www.noisebridge.net/wiki/Donate),
[Tor](https://torproject.org) or the [Free Software Foundation](https://fsf.org/)
. Because they *do* do some cool stuff.

## Our Goals

###Our goal here is straightforward:

+ Isolate tor within a FreeBSD-jail
  + With the help of EZJail

+ Create rules for our firewall that will transfer relevant connections
  + That is to say, transfer any connection on port 9001 to & from our jail

+ Create an ideal jail-configuration inwhich our relay will live out it's days.

###We will achieve this by:

+ Using a computer
  + With a keyboard
+ Anxiety (optional)
+ Watch some King of the Hill

* * * 

For those of you who want to follow along at home while reading this
<del>terrific</del> guide, *I will assume that you have atleast done the following:*

+ Installed **FreeBSD**
  + Installed ezjail
  + You know how to edit textfiles from a commandline
  + Got root-access

With that said, let's get started :D

# 1.0 - Groundwork

Laying the groundwork for our setup will begin in crafting a suitable *flavour* of a
FreeBSD-jail which will house our tor-Relay.

Assuming you've just installed ezjail, the first command to execute is
`# ezjail-admin install -p`, which will populate a baseinstall that any jail will share
with other jails (it will take 3-5 minutes, go make some coffee). **You can probably
skip this command if you've done any work with ezjail before.**

You can confirm the existance of the basejail & so-on by looking in `/usr/jails`

~~~
root@bananas:/usr/jails # ls -lsa
total 43
9 drwxr-xr-x   5 root  wheel   5 Oct 28 10:45 .
9 drwxr-xr-x  18 root  wheel  18 Oct 28 10:43 ..
9 drwxr-xr-x   9 root  wheel   9 Oct 28 10:45 basejail
9 drwxr-xr-x   4 root  wheel   4 Oct 28 10:49 flavours
9 drwxr-xr-x  12 root  wheel  22 Oct 28 10:45 newjail
~~~

## 1.1 - Host configuration

Some changes to `rc.conf` on the host-machine is needed to setup networking for our
jails. We want to make our network-interface to have aliases which our jails can bind to.

Fire up your editor-of-choice & insert the following lines into `/etc/rc.conf`.

~~~
## Enable ezjail startup on boot
ezjail_enable="YES"

## Aliases for jails
ifconfig_igb0_alias0="inet 192.168.1.100 netmask 255.255.255.0" # Relay
ifconfig_igb0_alias1="inet 192.168.1.101 netmask 255.255.255.0" # Another Jail
~~~

In this case, the `igb0` references the network-interface that connects to the internet.
Make sure that you change this to correspond to your machines network-interface.

These settings can be directly setup using `# ifconfig YOUR_INTERFACE_NAME_HERE ALIAS_IP alias`
or we can just restart the `netif`-service by running `# service netif restart`.

We are able to remove these aliases by running
`# ifconfig YOUR_INTERFACE_NAME_HERE ALIAS_IP -alias` & confirm that these changes
infact were implemented.

~~~
 » ifconfig
igb0: flags=8843<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST> metric 0 mtu 1500
        options=403bb<RXCSUM,TXCSUM,VLAN_MTU,VLAN_HWTAGGING,JUMBO_MTU,VLAN_HWCSUM,TSO4,TSO6,VLAN_HWTSO>
        ether d0:50:99:1d:ba:af
        inet 192.168.1.91 netmask 0xffffff00 broadcast 192.168.1.255
        media: Ethernet autoselect (1000baseT <full-duplex>)
        status: active
lo0: flags=8049<UP,LOOPBACK,RUNNING,MULTICAST> metric 0 mtu 16384
        options=600003<RXCSUM,TXCSUM,RXCSUM_IPV6,TXCSUM_IPV6>
        inet 127.0.0.1 netmask 0xff000000
pflog0: flags=100<PROMISC> metric 0 mtu 33160
~~~

## 1.2 - Flavours of jails that go with onions
At this point we go into `/usr/jails/flavours` & copy the example flavour into
`vanilla`, the name vanilla is arbitrary & feel free to adapt it however you want.

~~~
# cd /usr/jails/flavours
# cp -r example vanilla
~~~

Jail-flavours are basically files applied to the jail, a flavour is applied when
creating the jail. In a nutshell, a flavour of a particular jail is just files that
will be transfered into it. So if our flavour requires the line `hostname="NAS"` in
`/etc/rc.conf`, we apply said line within `/usr/jails/flavours/FLAVOUR_NAME`.

When we create a jail using `# ezjail-admin create`, we can pass `-f FLAVOUR_NAME` &
ezjail will create a jail based on our flavour.

So *Whoop-dee-doo*, we got ourself our very own flavour of jails, edit
`vanilla/etc/rc.conf` & make it look like the following:

~~~
# ???
network_interfaces=""

# Prevent rpc
rpcbind_enable="NO"

# Prevent loads of jails doing their cron jobs at the same time
cron_flags="$cron_flags -J 15"

# Prevent syslog to open sockets
syslogd_flags="-ss"

# Prevent sendmail to try to connect to localhost
sendmail_enable="NO"
sendmail_submit_enable="NO"
sendmail_outbound_enable="NO"
sendmail_msp_queue_enable="NO"

# No SSH
sshd_enable="NO"

# Tor
tor_enable="YES"

# Security
kern_securelevel_enable="YES"
kern_securelevel="1"

# Clear tmp
clear_tmp_enable="YES
~~~

The comments in the above file should leave some things pretty clear & other things
not so much, do not worry about it. 

Continuing our textual adventures, we replace any reference to `example` in the script
located in `flavours/vanilla/etc/rc.d`. When a flavour of our jail is initialized, this
script will be ran & then removed, and we can change the file so as to install an
editor after the jail is setup. Useful if you do not like or are familiar with vi as
an texteditor.
`# cd flavours/vanilla/etc/rc.d`,
`# cat ezjail.flavour.example | sed 's/example/vanilla/b' > ezjail.flavour.vanilla` &
finally remove the original script. `# rm ezjail.flavour.example`.

Take a few minutes to read `ezjail.flavour.example`, as it's filled with comments that
might be of use to you.

* * *

### Footnotes

+ Shoutouts to **hukl** & his awesome series on FreeBSD-stuff on vimeo, which you can
watch [here](http://vimeo.com/85855686)

+ If, at any point, you want to remove any or all jails you can do something like:
`# cd /usr`, `# chflags -R noschg ./jails` & finally `# rm -rf ./jails`. This will
effectively reset the situation to a state before any jail or basejail existed.
This requires `# ezjail-admin install -p` to be ran, as to repopulate the basejail.
 
