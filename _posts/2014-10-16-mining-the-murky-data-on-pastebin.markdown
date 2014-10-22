---
layout: post
title:  "Mining The Murky Data On Pastebin"
---

# What is this, I dont even
In this post we are going to basically construct a program together, or I will atleast try to tell you, the reader how I did it!

A very brief `$ whoami`, I started programming in Python3 around summer 2013, so I have at the time of writing this, a little more than a years experience of casual coding using Python3.
I have dabbled in C & [Processing](http://processing.org).

So take this post for what it is. And if you think I'm factually wrong in any of my statements, you can [send me a tweet][contact] & I will try to correct that.

> #TL;DR:
> I will try to create a scraper using Golang. Coming from a background in Python3.

I recently bought the book [Programming in Go](http://www.amazon.com/Programming-Go-Creating-Applications-Developers/dp/0321774639/), I'm excited to start getting my hands dirty with
programs like crawlers, which i enjoy writing & hacking on.

So if you are thinking about getting started with Golang, but you've only written code in Python or a similiar language before, we might be on a similiar level!

If I somehow sold you, lets get started.

* * * 

##<del>Upload</del> Download all the things!
You have probably heard about [pastebin][pbin], a place where data still roams free & snippets of exceptions, chatlogs
and leaks come together to form the computing gene-pool of our time.

We share our exceptions & bugs there, to get help from, or show other people on the internet. Some people upload hacks or leaks on pastebin & others
upload what appears to be nothing more than write-ups or .txt's about Pokémons-movesets, poems & everything inbetween. **So we download it all**

![we need to crawl for more rims]({{ site-url }}/assets/stupid-rims.jpg)_Analogy_

*Our goal is to create a crawler that can download all the latest things from pastebin, with concurrency & all sorts of flashy things that Go can enable us to implement.*

#You can't spell programming without 0x4f, 0x47.
Wouldnt it be great if we were to make a concurrent crawler in [Go][golang] that fetches raw txt uploaded to pastebin & saves them for future experimentation? I know, right?
The thing is that we can at the URL-level make requests to pastebin that lets us avoid parsing HTML alltogether.
Just take a look at the [pastebin API][pbinapi]

>#Getting paste raw output
>This option is actually not part of our API, but you might still want to use it. To get the RAW output of a paste you can use our RAW data output URL:
>`http://pastebin.com/raw.php?i=`
>Simply add the paste_key at the end of that URL and you will get the RAW output.

**Yay! no html-parsing to just fetch a .txt!**

### Except, well..
**We still need to parse some html.**
The problem is, our crawler wont be able to get any actual _paste_key_'s without scraping some good ol' html. 
At this point I looked if I could find any project similiar to [BeautifulSoup](http://www.crummy.com/software/BeautifulSoup/) (Python) where any and all parsing magic is
ready for us to <del>ignore</del> dive into. `http`, along with `regexp` & pals from the [go standard library](http://golang.org/pkg/) will make our program do what we want.

### Maybe we dont need a parsing-library, _it's just some href's_

We can take a look in how pastebin builds it's html which displays recent public pastes.
{% highlight html %}
<tr>
    <td>
        <img class="i_p0" border="0" alt="" src="/i/t.gif"></img>
        <a href="/DKhxef4N"></a> <!-- We want the value of this href -->
	</td>
	<td></td>
	<td align="right"></td>
</tr>
{% endhighlight %}

These href's link to the pastes themselves, so thats the entrypoint for our crawler for now. And it should be pretty simple to parse these href's out of the html, afterall, we can use
regular expressions to find these little bastards!

> I had a problem, so I decided to use regular expressions. Now i have two problems.

With the help of [GoRegex](http://goregex.com/) along with [Duckduckgo's handy cheatsheet](https://duckduckgo.com/?q=regex), we are able to quickly throw together a regex that matches a selected sample.

`<a href="/[a-zA-Z0-9]+">` or `<a href="/[a-zA-Z0-9]{8}">`, where `{8}` denotes that we are sure that we will only encounter an 8-character link. This matches against our example html straight from [pastebin][pbin],
`<a href="/DKhxef4N">` where `DKhxef4N` is the previously mentioned `paste_key`. One possible pitfal with this quick-n-dirty solution is that we might end up catching links like `<a href="/trends">` and other href's found on pastebin.
We can address this in our code later, where we skip those links. Allthough I'm sure that my haphazardly put together regex could be crafted to differenciate `/trends` from `/DKhxef4N`, I'm not much of a regex-ninja.
But if you are & can help me out, [let me know][contact]

# Gerbals & snakes live seperate lives.

Lets get some code going, just to get our feet wet with Golang.
Beginning our pastecrawler.go is some basic Golang structure where we define what package our code is a part of,
what `import`s we use & our (for now) basic `main()`-function.

{% highlight go %}
package main

import (
	"fmt"
	"log"
	"net/http"
	"io/ioutil"
)

func main() {
	getLatest()
}
{% endhighlight %}

Right now it's really simple! However familiar you may be with programming in Go, we can run through the basics.
`package`-syntax

`import`s are basically the dependencies of our program, any unused dependency in golang makes the compiler angry at us.
If we would have imported `io/ioutil` & not used it later in our program, we would have recieved an "unused dependency"-error when we tried to compile.
But you probably know that, and if you feel like you dont have a clue what im talking about, I would suggests taking the [tour of go](http://tour.golang.org/#1).

# How do i computer?

In a nutshell, the program will consist of a infinite loop inwhich we call the following functions:

+ `getLatest()`  
  `getLatest` will make a `GET`-request to pastebin.com/latest & return a response along with an error.  
  Provided everything went ok, we take the output & feed it into `produce()`

+ `produce()`  
  The purpose of `produce()` is for it to act like an producer for the function `download()`  
  Using `go`-routines[^1] on a nested, anonymous function, which passes parsed pastekeys into a channel.
  `produce` parses out pastekeys using the functions `extract` & `contains`

  + `extract` takes string as parameter & applies some basic parsing on said string.  
  returns an empty string if no match was found.

  + `contains` is just a function that iterates over a slice[^2] of strings and checks if a given string is in said slice.
	Useful when we want to avoid href's that do not refer to pastes, such as `/about`, `/latest`


+ `download()`  
  

## getLatest()
{% highlight go %}

func getLatest() (string, error) {
	// Request the /archive-page
	resp, reqError := http.Get("http://pastebin.com/archive")

	// Make sure we close our response.
	defer resp.Body.Close()

  	// Error-check
	if reqError != nil {
		log.Fatal(reqError)
		return nil, reqError
	}

	// Print the status-code on our response
	fmt.Printf("Status-code: %s", resp.Status)
	
	// Read the body
	body, readError := ioutil.ReadAll(resp.Body)

	// Error-check
	if readError != nil {
		log.Fatal(readError)
		return nil, readError
	}

	return string(body), nil
}
{% endhighlight %}

We use the `defer`-statement to keep a tidy ship, where the `defer`-statement is what will be called before our function returns control to its caller (in this case `main()`)
It's in other words, to make sure our connection (referenced by `resp`) in this case, is closed properly.

We use the golang's nice type-converting in our return-statement. Where we take `body` of type `[]byte` (slife of bytes) & simply run it through `string()`.

## produce()
It would be a sin not to experiment with golang's excellent & easily implemented concurrency!
So our program will need a producer, a function that basically shoves freshly parsed paste_keys into a *channel* that our other functions read from.

## extract()
If we take a look at our new function, that will be doing some parsing-magic, it's really nothing special.
Collectively it took around an hour or two for me to construct & I had to do a fair amount of banging my head against the wall when i got stuck (did i tell you i just started writing Go?).
{% highlight go %}
func extract(rawln string) (parsedln string) {

	// Our static regex, used for zeroing in on hrefs
	regex := regexp.MustCompile("href=\"/[a-zA-Z0-9]{6,10}\"")

	// Ignore blank matches
	if match := regex.FindString(rawln); match != "" {
		// Now we take out whats inbetween the quote-marks, the
		// remaining text at this point is: href="/", let's remove it
		parsedln := strings.Trim(match, "href=\"/\"")
		return parsedln
	}

	return ""
}
{% endhighlight %}
Our function takes a string referenced by `rawln`, and gives us a parsed string back `parsedln`.
For now, since our applications goal is to simply grab some href's off of pastebins website, our regex is hardcoded in there & tested to work like we want it.
In the future we might add more "modes" that would enable our programs users to scrape another, similiar website (there are [tons of them](http://alternativeto.net/software/pastebin/)).

We create a regexp object that we can use methods like `FindString`, `Match`, etc.
As commented in the code, we also just chop away some stuff we dont need in the strings using `strings.Trim`.

One of the things i really like about golang at this point is manifested in the line `if match := regex.FindString(rawln); match != "" { ...`, where we declare a variable & based on what that variable is, we can act accordingly. But wait! **There's more**:  
Not only is it clear & concise, the variables declared in our `if`-statement is limited to that block of code, So if we write something like:

{% highlight go %}
if match := regex.FindString(rawln); match != "" {
	// Do stuff
}
fmt.Println(match)
{% endhighlight %}

It wont compile, since the variable `match` is undefined, limited to the scope of the `if`-statement.

## contains()

We need to keep track of the URL's we have downloaded, we wouldn't want to download anything more times than whats needed. 
This can be achieved by appending the pastekeys to a blacklist as we download them.

So we declare a slice of strings inwhich to put our blacklisted pastekeys. We also pass in some default values that we want to ignore.

{% highlight go %}
var blacklist = []string{
	"trends",
	"archive",
	"signup",
	"tools",
}
{% endhighlight %}

Also note the slightly different syntax of declaring a variable outside a function or statement.
Instead of `:=`, we use the `var`-keyword here to declare our variable.

In python, we would be able to write `list().contains(object)`, but this is not necessarily the case in go.
There is no built-in function for checking this, but it's trivial to write a function that does this.

{% highlight go %}
func contains(slice []string, object string) bool {
	// Iterate over the items in referenced slice
	for _, sliceString := range slice {
		// If the iterator matches the object, we return true
		if sliceString == object {
			return true
		}
	}
	// When our loop stops, we can assume no match was found & return false
	return false
}
{% endhighlight %}
_Two variables are used when iterating over objects with the `range`-keyword, using `for index, item := range` syntax. In this case we define `index` as `_`,
a special variable-name in go which discards it's value._

By having to create our own contains-function, we sacrifice convenience (a generic contains-function able to compare data of any type) for speed.
It makes sense that a function that does not need to figure out the type of each piece of data we give it, is faster than
a specialised function.

Using our `contains` we can compare if a particular href have been visited or not. **Great!**


* * *
[^1]: Go test

[^2]: Slices in go are similar to lists in python. However, in go, we also need to declare what kind of data is going into the slice. If we declare a slice with content of a certain type, we cannot insert or append data which doesn't match the type declared, into the slice. in this case we declare the variable `blacklist` to be a slice of strings. [_Go's definition_](http://blog.golang.org/go-slices-usage-and-internals)


[pbin]:(http://pastebin.com)
[pbinapi]:(http://pastebin.com/api)
[golang]:(https://golang.org)
[contact]:(https://twitter.com/jonatanhal)
