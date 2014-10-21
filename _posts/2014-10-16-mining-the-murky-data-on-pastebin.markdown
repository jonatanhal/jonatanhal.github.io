---
layout: post
title:  "Mining the murky data on pastebin"
---

# What is this, i dont even
In this post we are going to basically construct a program together, or i will atleast try to tell you, the reader how I did it!
A very brief `jonatanhal 101`, i started programming in Python3 around summer 2013, so I have at the time of writing this, a little more than a years experience of casual coding using Python3.
I have dabbled in C & [Processing](http://processing.org).

So take this post for what it is. And if you think I'm factually wrong in any of my statements, you can [send me a tweet][contact] & I will try to correct that.

I recently bought the book [Programming in Go](http://www.amazon.com/Programming-Go-Creating-Applications-Developers/dp/0321774639/), and coming from a background in python I'm excited to start getting my hands dirty with
programs like crawlers, which i enjoy writing & hacking on.

> #TL;DR:
> I will try to create a scraper using Golang. Coming from a background in Python3.

So if you are thinking about getting started with Golang, but you've only written code in Python or a similiar language before, we might be on a similiar level!

If I somehow sold you, lets get started.

##<del>Upload</del> Download all the things!
You have probably heard about [pastebin][pbin], a place where data still roams free & snippets of exceptions, chatlogs
and leaks come together to form the computing gene-pool of our time.

We share our exceptions & bugs here, to get help from other people on the internet that share our interest in the turbo-pascal-IDE. Some people upload hacks or leaks on pastebin & others
upload what appears to be nothing more than write-ups or .txt's about Pokémons-movesets, poems & everything inbetween.

![we need to crawl for more rims]({{ site-url }}/assets/stupid-rims.jpg)  
_Analogy_

So our goal is to create a crawler that can download _things_ from pastebin, with concurrency & all sorts of flashy things that Go can enable us to implement.

#You cant spell programming without O & G.
Wouldnt it be great if we were to make a concurrent crawler in [Go][golang] that fetches raw txt uploaded to pastebin & saves them for future experimentation? I know, right?
The thing is that we can at the URL-level make requests to pastebin that lets us avoid parsing HTML alltogether.
Just take a look at the [pastebin API][pbinapi]

>#Getting paste raw output
>This option is actually not part of our API, but you might still want to use it. To get the RAW output of a paste you can use our RAW data output URL:
>`http://pastebin.com/raw.php?i=`
>Simply add the paste_key at the end of that URL and you will get the RAW output.

_Yay! no html-parsing to just fetch a .txt!_

### Except, well.. we still need to parse some html.
The problem occurs right away & I realize that our crawler wont be able to get any actual __paste_key's__ without scraping some good ol' html.
At this point I looked if I could find any project similiar to [BeautifulSoup](http://www.crummy.com/software/BeautifulSoup/) (Python) where any and all parsing magic is
vready for us to <del>ignore</del> dive into. `http`, along with `regexp` & pals from the standard go-library will make our program do what we want.

### Maybe we dont need a parsing-library, _it's just some href's_

We can take a look in how pastebin builds it's html which displays recent public pastes.
{% highlight html %}
<table class="maintable" cellspacing="0">

    <tbody>
        <tr class="top">
            <th align="left" scope="col"></th>
            <th align="left" scope="col"></th>
            <th align="right" scope="col"></th>
        </tr>
        <tr>
            <td>
                <img class="i_p0" border="0" alt="" src="/i/t.gif"></img>
                <a href="/DKhxef4N"></a> <!-- We want this href -->
            </td>
            <td></td>
            <td align="right"></td>
        </tr>
	...
{% endhighlight %}

These href's link to the pastes themselves, so thats the entrypoint for our crawler for now. And it should be pretty simple to parse these href's out of the .html, since we can use
regex to find these little bastards.

With the help of [GoRegex](http://goregex.com/) we are able to quickly throw together a regex that matches a selected sample.

`<a href="/[a-zA-Z0-9]+">` or `<a href="/[a-zA-Z0-9]{8}">`, where `{8}` denotes that we are sure that we will only encounter an 8-character link. This matches against our example html straight from [pastebin][pbin],
`<a href="/DKhxef4N">` where `DKhxef4N` is the previously mentioned `paste_key`. One possible pitfal with this quick-n-dirty solution is that we might end up catching links like `<a href="/trends">` and other href's found on pastebin.
We can address this in our code later, where we skip those links. Allthough I'm sure that my haphazardly put together regex could be crafted to differenciate `/trends` from `/DKhxef4N`, I'm not much of a regex-ninja.
But if you are & can help me out, [let me know][contact]

# The Marriage of a Python & a Gopher

![gopher]({{ site-url }}/assets/project.png)  
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
  Using `go`-routines on a nested, anonymous function, which passes parsed pastekeys into a channel.
  `produce` parses out pastekeys using the functions `extract` & `contains`

  + `extract` takes string as parameter & applies some basic parsing on said string.  
  returns an empty string if no match was found.

  + `contains` is just a function that iterates over a slice of strings and checks if a given string is in said slice.
	Useful when we want to avoid href's such as "/about", "/contact" & so on.

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

If you are coming from Python & scratching your head, saying "WTF are slices?" - Slices in go
are similar to lists in python. In go, we also declare what kind of data is going into the slice.
We cannot declare a slice of strings & insert or append data which match the declared type.

in this case we declare it to be a slice of strings. 

A list in python would be able to accept data of any type.

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

At this point we can basically call our two functions in `main()` & rejoice that our code does something slightly cooler than it did before.

## contains()

We need to keep track of the URL's we have downloaded, we wouldn't want to download anything more times than whats needed.
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


[pbin]:(http://pastebin.com)
[pbinapi]:(http://pastebin.com/api)
[golang]:(https://golang.org)
[contact]:(https://twitter.com/jonatanhal)
