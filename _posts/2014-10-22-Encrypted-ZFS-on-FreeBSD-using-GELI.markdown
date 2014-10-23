---
layout: post
title:  "Setting up encrypted ZFS on FreeBSD using GELI"
---

# $ < post.md | grep why\?
When I first setup my FreeBSD NAS, I wanted to encrypt my data but still be able to
take advantage of [what ZFS brings](https://en.wikipedia.org/wiki/ZFS#Features).
Especially since my NAS has a couple of disks.

I advice you to read up on [geli](https://www.freebsd.org/cgi/man.cgi?query=geli)
& other relevant sources of information[^1] if you are going to do this. We make it convenient for
ourselves with some very basic functions in my `.zshrc` which take care of the tedious work of
re-initializing the drives when one needs to restart the host-machine

## But first, we need to set things up.
We start by partitioning our drives to a single partition using `gdisk`.
But before we do that, we wipe our drives so that we can be absolutely sure that no old
partition-table is present.

**Please note that this action will destroy the partitions on the hard-drive**

{% highlight sh %}
$ gdisk ada1
GPT fdisk (gdisk) version 0.8.10

Partition table scan:
	MBR: not present
	BSD: not present
	APM: not present
	GPT: not present

Command (? for help): x
Expert Command (? for help): z
About to wipe out GPT on /dev/ada1. Proceed (Y/N): y
{% endhighlight %}

Here, `x` enabled us to specify special commands. The command `z` simply tells gdisk to wipe
the partition-table on the given hard-drive & create a new GPT-header with a protective MBR.

Repeat this for every singe drive you want to incorporate in your encrypted ZFS pool.
At this point we add a single partition, which size is that of the whole hard-drive.

**As you probably have figured out, the following action will render the data on your
hard-drive hard to get back. Keep that in mind.**

{% highlight sh %}
$ gdisk ada1
GPT fdisk (gdisk) version 0.8.10

Partition table scan:
	MBR: not present
	BSD: not present
	APM: not present
	GPT: Present

Command (? for help): n
Partition number(2-128, default 1): # Just press enter
First sector 34-2047, default = 34) or {+-}size{KMGTP}: # Just press enter
Last sector 35-10231090, default 10231090) or {+-}size{KMGTP}: # Just press enter

Command (? for help): w
{% endhighlight %}

In our commands above, we simply create a single partition that encompasses the entire drive.
**Repeat this step for every hard drive you want to build your encrypted pool with.**

# A Key, A lock & GELI
So in my case, I wanted to encrypt my array of disks using a key. Geli allows the use of a key
along with a pass-phrase to be used to unlock or initialize your hard-drives.

Note that a different key & pass-phrase for each individual disk is possible, but I found this
unpractical for my application. If Security is paramount I would advice you to follow a
guide in a relevant scope. In my case the balance of security vs convenience tipped
in the favor of convenience when deciding to go with a single key & pass-phrase.

We create a key consisting of 256 random bits using `dd if=/dev/random of=./keyfile bs=256
count=1` **This will create a key-file, which you must keep in a safe place**. At this point
we initialize our drives using `geli init -s 4096 -K ./keyfile -l 256 /path/to/your/harddrive/
partition`. We will be prompted to enter a pass-phrase & in our case we use the same key-phrase
for all our drives. The options used in `geli init` boils down to `-s 4096` indicating
sector-size, `-K ./keyfile` is our key-file that geli will be using, along with a pass-phrase
to unlock & attach the hard drive. `-l 256` Tells geli what size our key is. **You should
replace `/path/to/your/harddrive/partition` with your actual device-path, for example
`/dev/ada2p1`**

## Now we've initialized our hard-drives, lets attach them
Attaching geli-encrypted devices is similar to `cryptsetup open` in Linux. We are basically
unlocking them for use, and we do this by issuing the the command `geli attach -k ./keyfile
/path/to/your/harddrive/partition`. We will be prompted to enter our pass-phrase again, if
everything went well & keys along with pass-phrases were provided correctly, Geli will have
created an interface for our encrypted drive located in `/dev`.

For example, I attach the drive `/dev/ada1p1` & geli creates `/dev/ada1p1.eli`.

We can now rejoice & finally create our ZFS-pool in raidz & enjoy the other fancy features of
ZFS.

## When my hard-drives die, my ticket to Frowntown will let me get out of there
With our newly fashioned, encrypted hard-drives we tell ZFS to do it's magic.
`zpool create storage raidz ada1p1.eli ada2p1.eli ada3p1.eli`
**Note that `storage` only is the chosen name of our pool, feel free to change this to whatever
you want :D**

**When first setting up your ZFS raidz, remember to include all your prepared drives**

# Doing things by hand over & over sucks, let's make it EZ (but probably insecure).
Modifying our *.zshrc*, we can implement some convenient functions that would make
any cryptographer or paranoid person cringe & sub-tweet how horrible infosec-practice this is.

*Note that since geli is detaching any attached drive when we reboot our machine. We wouldn't
want to not be ready for that. So before we reboot, power-off our machine or detach any
geli-attached drive, we **export** our pool, using the `geliclose`-function mentioned below* 

{% highlight sh %}
function geliopen() {

    # Read password
    echo "OK, nobody is listening. Secret is... ?";
    read -rs PASS && echo $PASS > ./.pfile;
    
    geliattach ada1p1
    geliattach ada2p1
    geliattach ada3p1
    geliattach ada4p1
    geliattach ada5p1

    sudo zpool import encrypted && echo "imported"

    # Reset password & remove file
    unset PASS && rm -P ./.pfile || echo "Failed to unset PASS and/or removing .pfile"
    
    cd;
}
{% endhighlight %}

We read a given pass-phrase into the variable `PASS` & echo the variable into a file.
Then basically calls another function, `geliattach` which attaches a drive. The
difference here is that the `-j ./.pfile`, which tells geli to read the pass-phrase from a file
instead of prompting the user for input.

{% highlight sh %}
function geliattach() {
    $(sudo geli attach -k /usr/home/user/.keyfile -j ./.pfile /dev/$1 && echo $1) || "failed attaching";
}
{% endhighlight %}

When all is good & the drives have been attached correctly, we use the magic of ZFS to import
our exported pool.

{% highlight sh %}
function geliclose() {

    echo " @@@@@@@@@@@@@@@@@@@";
    echo " @  W A R N I N G  @";
    echo " @-----------------@";
    echo " @ ABOUT TO EXPORT @"
    echo " @@@@@@@@@@@@@@@@@@@";
    read;

    sync;
    sudo zpool export encrypted;

    sudo geli detach ada1p1;
    sudo geli detach ada2p1;
    sudo geli detach ada3p1;
    sudo geli detach ada4p1;
    sudo geli detach ada5p1;
}
{% endhighlight %}

Last, but not least, is our function that takes care of making sure we a), warn the user that
we are about to export the ZFS pool, b), exports the pool & finally c) detach all geli attached
drives. Which makes us able to do stuff like reboot, restart & gtfo.

* * *

#In summary
Walking the tightrope of security vs convenience is tricky, I think that there's definitely
room for improvements in the way my functions handle variables & files when unlocking and
handling geli-encrypted disks.

**I must emphasize: I am convinced that the above mentioned handling of encrypted disks is
not secure, but it is more convenient. If you spot some flaw; [Please let me know](https://twitter.com/jonatanhal)**

+ Even though passing `-P` to `rm` for overwriting the pass-phrase-file with zeroes, there's a
good chance that the data from the pass-phrase file might still be there. **If you are using
ZFS, there's a good chance its much more tricky (if not impossible) making sure that data
disappears**. An option would be to just echo the pass-phrase & piping the output to geli,
which reads the pass-phrase from std-in.

+ I don't know if simply unsetting the variable `PASS` will clear it's location in memory. I am
assuming that our shell of choice frees the variable's place in memory.

* * *
[^1]: Relevant information: [zfs man page](https://www.freebsd.org/cgi/man.cgi?query=zfs&apropos=0&sektion=0&manpath=FreeBSD+10.1-RELEASE&arch=default&format=html), [zpool manpange](https://www.freebsd.org/cgi/man.cgi?query=zpool&apropos=0&sektion=0&manpath=FreeBSD+10.1-RELEASE&arch=default&format=html), [Freebsd Handbook on disk-encryption, skip to 18.12.2 for information about geli](https://www.freebsd.org/doc/en/books/handbook/disks-encrypting.html)

