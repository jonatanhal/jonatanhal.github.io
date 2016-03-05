---

layout: post
title:  "Protostar Exploit Exercises: Net challenges"
author: jonatanhal

---

## [net0](https://exploit-exercises.com/protostar/net0)

>## About
>
>This level takes a look at converting strings to little endian integers.

This level is easily solvable via python.

In previous levels, I've been using the `struct.pack` function in
python to convert values to their escaped counterpart. For example,
the hexadecimal value `0xdeadbeef` is written as `"\xef\xbe\xad\xde"`
in python.

`struct.pack` takes care of converting a integer to this escaped
format.

Using the knowledge above and with a dash of parsing & sockets - We're
able to quickly solve this level.

{% highlight python %}
# net0_solution.py

import socket, struct

HOST="localhost"
PORT=2999

# setup a socket & connect to the running application
s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))

# recieve 128 bytes
output=s.recv(128)
# parse said bytes to only include the number to send.
start=output.find("'")
end=output.rfind("'")
s.sendall(struct.pack('i', int(output[start+1:end])))
print(s.recv(128))
{% endhighlight %}


~~~
$ python net0_solution.py
Thank you sir/madam
~~~

* * * 

## [net1](https://exploit-exercises.com/protostar/net1)

>## About
>
>This level tests the ability to convert binary integers into ascii representation.

In the previous challenge, we converted a ascii integer into a binary
representation. This time, we do the opposite.

{% highlight python %}
# net1_solution.py
import socket, struct

HOST="localhost"
PORT=2998

s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))
output=s.recv(4)
s.sendall(str(struct.unpack('I', output)[0]))

print(s.recv(128))

{% endhighlight %}

Note that we are using `struct.unpack` instead of `struct.pack` this time.

~~~
$ python net1_solution.py
you correctly sent the data
~~~

* * * 

## [net2](https://exploit-exercises.com/protostar/net2)

>## About
>
>This code tests the ability to add up 4 unsigned 32-bit integers. Hint: Keep in mind that it wraps.

Modifying our previous solution

{% highlight python %}
# net2_solution.py
import socket, struct

HOST="localhost"
PORT=2997

# Setup the connection
s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))

# Recieve & convert 4 bytes at a time.
a=int(struct.unpack('I', s.recv(4))[0])
b=int(struct.unpack('I', s.recv(4))[0])
c=int(struct.unpack('I', s.recv(4))[0])
d=int(struct.unpack('I', s.recv(4))[0])
# Calculate & Send back the result
s.sendall(struct.pack('I', a+b+c+d))

print(s.recv(128))

{% endhighlight %}

~~~
$ python net2_solution.py
you added them correctly
~~~

* * * 

These challenges were pretty simple to solve with the help of python. Thanks for reading.
