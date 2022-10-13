# Socket Treasure Hunt

The purpose of this assignment is to help you become more familiar with the
concepts associated with sockets, including UDP communications, local and
remote port assignment, IPv4 and IPv6, message parsing, and more.

# Table of Contents

 - [Overview](#overview)
 - [Preparation](#preparation)
   - [Reading](#reading)
 - [Instructions](#instructions)
   - [Level 0](#level-0)
   - [Level 1](#level-1)
   - [Level 2](#level-2)
   - [Level 3](#level-3)
   - [Level 4](#level-4)
 - [Helps and Hints](#helps-and-hints)
   - [Message Formatting](#message-formatting)
   - [Error Codes](#error-codes)
   - [Op Codes](#op-codes)
   - [UDP Socket Behaviors](#udp-socket-behaviors)
   - [Testing Servers](#testing-servers)
 - [Automated Testing](#automated-testing)
 - [Evaluation](#evaluation)
 - [Submission](#submission)


# Overview

This lab involves a game between two parties: the *client* and the *server*.
The server runs on a CS lab machine, awaiting incoming communications.
The client, also running on a CS lab machine, initiates communications with the
server, requesting the first chunk of the treasure, as well as directions to
get the next chunk.  The client and server continue this pattern of requesting
direction and following direction, until the full treasure has been received.
Your job is to write the client.


# Preparation


## Reading

Read the following in preparation for this assignment:
  - The man pages for the following:
    - `udp`
    - `ip`
    - `ipv6`
    - `socket`
    - `socket()`
    - `send()`
    - `recv()`
    - `bind()`
    - `getaddrinfo()`
    - `htons()`
    - `ntohs()`
    - `getpeername()`
    - `getsockname()`
    - `getnameinfo()`


# Instructions


## Level 0

First, open `treasure_hunter.c` and look around.  You will note that there are
two functions and one `#define` statement.

Now, replace `PUT_USERID_HERE` with your numerical user ID, which you can find
by running `id -u` on a CS lab machine.  From now on you can use `USERID` as an
integer wherever you need to use your user ID in the code.

Now take a look the `print_bytes()` function.  This function was created to
help you see what is in a given message that is about to be sent or has just
been received.  It is called by providing a pointer to a memory location, e.g.,
an array of `unsigned char`, and a length.  It then prints the hexadecimal
value for each byte, as well as the ASCII character equivalent for values less
than 128 (see `man ascii`).


### Command-Line Arguments

Your program should have the following usage:

```
$ ./treasure_hunter server port level seed
```

 - `server`: the domain name of the server.
 - `port`: the port on which the server is expecting initial communications.
 - `level`: the level to follow, a value between 0 and 4.
 - `seed`: an integer used to initialize the pseudo-random number generator on
   the server.

Store each of the arguments provided on the command line in variables.  Note
that `port`, `level`, and `seed` are all integers, so you will want to convert
them to integers. However, because `getaddrinfo()` takes a `char *` for port,
you might want to maintain a string (`char []`) version of the port as well.

It would be a good idea here to check that all command-line variables have been
stored appropriately from the command line.  Create some print statements to
that effect.  Then build your program by running the following:

```bash
$ make
```

Then run it:

```bash
$ ./treasure_hunter server 32400 0 7719
```

Try running it with different values for server, port, level, and seed to make
sure everything looks as it should.


### Initial Request

The very first message that the client sends to the server should be exactly
eight bytes long and have the following format:

<table border="1">
<tr>
<th>00</th><th>01</th><th>02</th><th>03</th><th>04</th><th>05</th><th>06</th><th>07</th></tr>
<tr>
<td colspan="1">0</td>
<td colspan="1">Level</td>
<td colspan="4">User ID</td>
<td colspan="2">Seed</td></tr>
</table>

The following is an explanation of each field:

 - Byte 0: 0
 - Byte 1: an integer 0 through 4, corresponding to the *level* of the
   course.  This comes from the command line.
 - Bytes 2 - 5: a `unsigned int` corresponding to the user ID of the user in
   network byte order (i.e., big-endian, most significant bytes first).  This
   can be retrieved with `id -u` on one of the CS lab machines.

   Since you used the`#define` directive to map `USERID` to your user ID, you
   can populate this with the value of `USERID`.
 - Bytes 6 - 7: an `unsigned short` used, along with the user ID and the level,
   to seed the pseudo-random number generator used by the server, in network
   byte order.  This comes from the command line.

   This is used to allow the client to experience consistent behavior every
   time they interact with the server, to help with development and
   troubleshooting.

In the `main()` function, declare an array of `unsigned char` to hold the bytes
that will comprise your request.  Populate that array with values from the
command line.

You might be wondering how to populate the array of `unsigned char` with values
longer than bytes.  Please take a moment to read the section on
[message formatting](#message-formatting), which will provide some background
and some examples for building your message.

Now call `print_bytes()`, specifying the message buffer that you have populated
as the first argument and 8 as your second argument (i.e., because it should be
an 8-byte message).

Re-build and run your file with the following:

```bash
$ make
$ ./treasure_hunter server 32400 0 7719
```

Check the `print_bytes()` output to make sure that the message you intend to
send looks correct.  Bytes 0 and 1 should both be zero.  Bytes 2 through 5
should have the value of your user ID in network byte order.  Bytes 6 and 7
should have the value of your seed in network byte order.  For example assuming
a user ID of 12345, the output would be:

```bash
$ ./treasure_hunter server 32400 0 7719

00: 00 00 00 00  30 39 1E 27  . . . . 0 9 . '
```

Beyond the offset (`00`), you can see the byte representing level 0 (`00`), the
bytes representing the user ID (`00 00 30 39`), and the bytes representing the
seed (`1E 27`).  Because this message is not "text", there are no useful ASCII
representations of the byte values, so the output is mostly `.`.  Note that you
can find the hexadecimal representation of your user ID, the seed, and any
other integer, by running the following:

```bash
$ printf "%08x\n" 12345
```

substituting `12345` with the integer that you wish you represent as
hexadecimal.

If your message looks good, based on the output to `print_bytes()`, continue
on!

Of course, you have not sent or received any messages at this point, but you
now know how to *format* the initial message appropriately.


### Sending and Receiving

Note: you will find working code examples for this section and others in the
[sockets homework assignment](../hw-sockets).

With your first message created, set up a UDP client socket, with
`getaddrinfo()` and `socket()`, specifying `AF_INET` and `SOCK_DGRAM` as the
address family and socket type, respectively.

When everything is set up, send your message using `sendto()`.  Then read the
server's response with `recvfrom()` call.  Remember, it is just one call to
each!  Store the return value of  `recvfrom()`, which reflects the number of
bytes you received.  Unlike the [initial request](#create-an-initial-request)
you sent, which is always eight bytes, the size of the response is variable
(but will never be more than 256 bytes).  Finally, call `print_bytes()` to
print out the contents of the message received by the server.

Re-build and re-run your program:

```bash
$ make
$ ./treasure_hunter server 32400 0 7719
```

At this point, you need to supply the name of an actual server. See
[this section](#testing-servers) for a list of servers and ports that you may
use.


### Directions Response

Take a look at the response from the server, as printed by `print_bytes()`.
Responses from the server are of variable length (but any given message will
consistent of fewer than 256 bytes) and will follow this format:

<table border="1">
<tr>
<th>00</th><th>01</th><th>::</th><th>n</th><th>n + 1</th><th>n + 2</th><th>n + 3</th><th>n + 4</th>
<th>n + 5</th><th>n + 6</th><th>n + 7</th></tr>
<tr>
<td colspan="1">Chunk Length</td>
<td colspan="3">Chunk</td>
<td colspan="1">Op Code</td>
<td colspan="2">Op Param</td>
<td colspan="4">Nonce</td></tr>
</table>

The following is an explanation of each field:

 - Byte 0: an `unsigned char`.
   - If 0: the hunt is over.  All chunks of the treasure have been received in
     previous messages from the server.  At this point the client can exit.
   - If between 1 and 127: A chunk of the message, having length corresponding
     to the value of byte 0, immediately follows, beginning with byte 1.
   - If greater than 127: The server detected an error and is alerting the
     client of the problem, with the value of the byte corresponding to the
     error encountered.  See [Error Codes](#error-codes) for more details.
   Note that in the case where byte 0 has value 0 or a value greater than 127,
   the entire message will only be one byte long.

 - Bytes 1 - `n` (where `n` matches the value of byte 0; only applies where `n`
   is between 1 and 127): The chunk of treasure that comes immediately after
   the one received most recently.

 - Byte `n + 1`: This is the op-code, i.e., the "directions" to follow to get
   the next chunk of treasure and the next nonce.  At this point, the op-code
   value you get from the server should be 0, which means "communicate with the
   server the same way you did before" or simply "no change." For future
   levels, this field will have values other than 0, each of which will
   correspond to a particular change that should be made with regard to how you
   contact the server.  See [Op Codes](#op-codes) for a summary, and the
   instructions for levels [1](#level-1), [2](#level-2), [3](#level-3), and
   [4](#level-4) for a detailed description of each.

 - Bytes `n + 2` - `n + 3`: These bytes, an `unsigned short` in network byte
   order is the parameter used in conjunction with the op-code.  For op-code 0,
   the field exists, but can simply be ignored.

 - Bytes `n + 4` - `n + 7`: These bytes, an `unsigned int` in network byte
   order, is a nonce.  The value of this nonce, plus one, should be returned in
   every communication back to the server.

Extract the chunk length, the chunk, treasure chunk, the op-code, the op-param,
and the nonce using the hints in the [message formatting](#message-formatting)
section), storing them in variables of the appropriate types, so you can work
with them.  Print them out to verify that you have extracted them properly, and
pay attention to endian-ness.  For example, if you receive the nonce
0x12345678, then printing out the value of the variable in which you have
stored the nonce, e.g., with:

```
printf("%x", nonce);
```

should result in the following output:

```
12345678
```

Remember to add a null byte after the treasure chunk, or `printf()` will not
know how to treat it properly.  Also, the op-param has no use for level 0, and
the value might actually be 0.  This means that endian-ness is hard to check at
this point.  But you can check it in future levels.

You will be sending the nonce (well, a variant of it) back to the server, in
exchange for additional chunks, until you have received the whole treasure.


### Follow-Up Request

After the initial request, every subsequent request will be exactly four bytes
and will have the following format:

<table border="1">
<tr>
<th>00</th><th>01</th><th>02</th><th>03</th></tr>
<tr>
<td colspan="4">Nonce + 1</td></tr>
</table>

 - Bytes 0 - 3: an `unsigned int` having a value of one more than the nonce
   most recently sent by the server, in network byte order.  For example, if
   the server previously sent 100, then this value would be 101.

Build your follow-up request using the guidance in the
[message formatting helps](#message-formatting) section, and use
`print_bytes()` to make sure it looks the way it should.  Re-build and re-run
your program:

```bash
$ make
$ ./treasure_hunter server 32400 0 7719
```

If everything looks good, then use `sendto()` to send your follow-up request
and `recvfrom()` to receive your next directions response.


### Program Output

Now generalize the pattern of sending
[follow-up requests](#follow-up-request) in response to
[directions responses](#directions-response), receiving the entire treasure,
one chunk at a time.  Add each new chunk received to the treasure you already
have.  You will want to create a loop with the appropriate termination test
indicating that the entire treasure has been received
(i.e., [byte 0 has a value of 0](#directions-response)).

Once your client has collected all of the treasure chunks, it should print the
entire treasure to standard output, followed by a newline (`\n`).  For example,
if the treasure hunt yielded the following chunks:

 - `abc`
 - `de`
 - `fghij`

Then the output would be:

```
abcdefghij
```

No treasure will be longer than 1,024 characters, so you may use that as your
buffer size.  Remember to add a null byte to the characters comprising your, so
they can be used with `printf()`!

At this point, make sure that the treasure is the only program output.  Remove
print statements that you have added to your code for debugging by commenting
them out or otherwise taking them out of the code flow (e.g., with
`if (verbose)`).


### Checkpoint 0

At this point, you can also test your work with
[automated testing](#automated-testing).  Level 0 should work at this point.

Now would be a good time to save your work, if you haven't already.


## Level 1

With level 0 working, you have a general framework for client-server
communications.  The difference now is that the directions response will
contain real actions.

For level 1, responses from the server will use op-code 1.  The client should
be expected to do everything it did at level 0, but also to extract the value
that should be used as the new remote port from each
[directions response](#directions-response) and use that new port for future
communications with the server when op-code 1 is provided.  The remote port is
specified by bytes `n + 2` and `n + 3` in the
[directions response](#directions-response), which is an `unsigned short` in
network byte order (see [Message Formatting](#message-formatting)).

Some guidance follows as to how to use the new remote port in future
communications.

Because local and remote addresses and ports are stored in a
`struct sockaddr_in` (or `struct sockaddr_in6` for IPv6), one way that you
might keep always track of your address family (`AF_INET` or `AF_INET6`, for
IPv4 and IPv6, respectively), as well as remote ports, is by declaring the
following:

```c
	int af;
	struct sockaddr_in ipv4addr_remote;
	struct sockaddr_in6 ipv6addr_remote;
```

and maintaining them along the way.  Then you can pass this as an argument to
`sendto()` with each message sent.  For example:

```c
	socklen_t remote_addr_len = sizeof(struct sockaddr_storage);
	if (sendto(sfd, buf, n, 0, (struct sockaddr *) &ipv4addr_remote,
			remote_addr_len) < 0) {
		perror("sendto()");
	}
```

Note that `sendto()` takes type `struct sockaddr *` as the second-to-last
argument, allowing it to accept either `struct sockaddr_in` or
`struct sockaddr_in6`, with the caveat that the argument is *cast* as a
`struct sockaddr *`.  Per the man page for `bind()`, "The only purpose of this
structure (`struct sockaddr`) is to cast the structure pointer passed in `addr`
in order to avoid compiler warnings."  The length of a `struct
sockaddr_storage` is passed as the last argument, so `sendto()` knows the size
of the address structure it is working with.

For reference, the data structures used for holding local or remote address and
port information are defined as follows (see the man pages for `ip(7)` and
`ipv6(7)`, respectively).

For IPv4 (`AF_INET`):
```c
           struct sockaddr_in {
               sa_family_t    sin_family; /* address family: AF_INET */
               in_port_t      sin_port;   /* port in network byte order */
               struct in_addr sin_addr;   /* internet address */
           };

           /* Internet address. */
           struct in_addr {
               uint32_t       s_addr;     /* address in network byte order */
           };
```

For IPv6 (`AF_INET6`):
```c
           struct sockaddr_in6 {
               sa_family_t     sin6_family;   /* AF_INET6 */
               in_port_t       sin6_port;     /* port number */
               uint32_t        sin6_flowinfo; /* IPv6 flow information */
               struct in6_addr sin6_addr;     /* IPv6 address */
               uint32_t        sin6_scope_id; /* Scope ID (new in 2.4) */
           };

           struct in6_addr {
               unsigned char   s6_addr[16];   /* IPv6 address */
           };
```

Thus, the structures, might be initialized using `getaddrinfo()` with something
like this:

```c
	// populate ipv4addr_remote or ipv6addr_remote with address information
	// found in the struct addrinfo from getaddrinfo()
	af = rp->ai_family;
	if (af == AF_INET) {
		ipv4addr_remote = *(struct sockaddr_in *)rp->ai_addr;
	} else {
		ipv6addr_remote = *(struct sockaddr_in6 *)rp->ai_addr;
	}
```

Then the port might be updated using something like this:

```c
	ipv4addr_remote.sin_port = htons(port); // specific port
```

Note that the `sin_port` of the `struct sockaddr_in` member contains the port
in *network* byte ordering (See [Message Formatting](#message-formatting)),
hence the use of `htons()`.

The same for IPv6:

```c
	ipv6addr.sin6_port = htons(port); // specific port
```

Just as with level 0, have your code
[collect all the chunks and printing the entire treasure to standard output](#program-output).


### Checkpoint 1

At this point, you can also test your work with
[automated testing](#automated-testing).  Levels 0 and 1 should both work at
this point.

Now would be a good time to save your work, if you haven't already.


## Level 2

For level 2, responses from the server will select from op-codes 1 and 2 at random.
The client should be expected to do everything it did at levels 1 and 2, but
also to extract the value that should be used as the new local port from each
[directions response](#directions-response) and use that new local port for future
communications with the server when op-code 2 is provided.  The local port is
specified by bytes `n + 2` and `n + 3` in the
[directions response](#directions-response), which is an `unsigned short` in
network byte order (see [Message Formatting](#message-formatting)).

Some guidance follows as to how to use the new local port in future
communications.

Just as it was recommended that you store the remote address and port for
[level 1](#level-1), it is recommended that you keep track of local address and
ports in local variables:

```c
	int af;
	struct sockaddr_in ipv4addr_local;
	struct sockaddr_in6 ipv6addr_local;
```

and maintain them along the way.

Association of a local address and port to a socket is done with the `bind()`
function.  The following tips associated with `bind()` are not specific to UDP
sockets (type `SOCK_DGRAM`) but are nonetheless useful for this lab:

 - The local address and port can be associated with a socket using `bind()`.
   See the man pages for `udp` and `bind()`.
 - `bind()` can only be called *once* on a socket.  See the man page for
   `bind()`.
 - Even if `bind()` has *not* been called on a socket, if a local address and
   port have been associated with the socket implicitly (i.e., when `write()`,
   `send()`, or `sendto()` is called on that socket), `bind()` cannot be called
   on that socket.

Therefore, every time the client is told to use a new local port (i.e., with
op-code 2 in a directions response), then the *current socket must be closed*,
and a new one must be created.  Then `bind()` is called on the new socket.

Because the kernel implicitly assigns an unused local address and port to a
given socket when none has been explicitly assigned using `bind()`, at times
you will find it useful to learn the port that has been assigned. To populate
the address structures that you've declared with the local address and port
information already associated with a given socket, use `getsockname()`.  For
example:

```c
	// populate ipv4addr_local or ipv6addr_local with address information
	// associated with sfd using getsockname()
	socklen_t addrlen;
	if (af == AF_INET) {
		addrlen = sizeof(struct sockaddr_in);
		getsockname(sfd, (struct sockaddr *)&ipv4addr_local, &addrlen);
	} else {
		addrlen = sizeof(struct sockaddr_in6);
		getsockname(sfd, (struct sockaddr *)&ipv6addr_local, &addrlen);
	}
```

So you can later run something like this to bind the newly-created socket to a
specific port.

```c
	if (af == AF_INET) {
		ipv4addr_local.sin_family = AF_INET; // use AF_INET (IPv4)
		ipv4addr_local.sin_port = htons(port); // specific port
		ipv4addr_local.sin_addr.s_addr = 0; // any/all local addresses
		if (bind(sfd, (struct sockaddr *)&ipv4addr_local,
				sizeof(struct sockaddr_in)) < 0) {
			perror("bind()");
		}
	} else {
		ipv6addr_local.sin6_family = AF_INET6; // IPv6 (AF_INET6)
		ipv6addr_local.sin6_port = htons(port); // specific port
		bzero(ipv6addr_local.sin6_addr.s6_addr, 16); // any/all local addresses
		if (bind(sfd, (struct sockaddr *)&ipv6addr_local,
				sizeof(struct sockaddr_in6)) < 0) {
			perror("bind()");
		}
	}

```

Note that `bind()` (like `sendto()`) takes type `struct sockaddr *` as the
second argument, allowing it to accept either `struct sockaddr_in` or
`struct sockaddr_in6`, with the caveat that the argument is *cast* as a
`struct sockaddr *`.  The length of the address structure (either `struct
sockaddr_in`  or `struct sockaddr_in6`) is passed as the last argument, so
`bind()` knows the size of the address structure it is working with.


### Checkpoint 2

At this point, you can also test your work with
[automated testing](#automated-testing).  Levels 0 through 2 should both work
at this point.

Now would be a good time to save your work, if you haven't already.


## Level 3

For level 2, responses from the server will select from op-codes 1 through 3 at
random.  The client should be expected to do everything it did at level 0
through 2, but should now also handle op-code 3.

With op-code 3, the client should read `m` datagrams from the socket (i.e.,
using the currently established local and remote ports), where `m` is specified
by the bytes `n + 2` and `n + 3`, which is an `unsigned short` in network byte
order.  While `m` takes up two bytes for consistency with the other op-codes,
its value will be only between 1 and 7.  Each of these datagrams will come from
a randomly-selected remote port on the server, so `recvfrom()` must be used by
the client to read them to determine which port they came from.

Each of the `m` datagrams received will have 0 length.  However, the contents
of the datagrams are not what is important; what is important is the remote
ports from which they originated.  The remote ports of the `m` datagrams should
be added together, and their sum is the nonce, whose value, plus 1, should be
returned with the next communication to the server.  Note that the sum of these
values might well sum to something that exceeds the 16 bits associated with an
`unsigned short` (16 bits), so you will want to store the sum with an `unsigned
int` (32 bits).


### Checkpoint 3

At this point, you can also test your work with
[automated testing](#automated-testing).  Levels 0 through 3 should both work
at this point.

Now would be a good time to save your work, if you haven't already.


## Level 4

For level 4, responses from the server will select from op-codes 1 through 4 at
random.  The client should be expected to do everything it did at level 0
through 3, but should now also handle op-code 4.

With op-code 4, the client should switch address families from using IPv4
(`AF_INET`) to IPv6 (`AF_INET6`) or vice-versa, and use the port specified by
the next two bytes (`n + 2` and `n + 3`), which is an `unsigned short` in
network byte order, as the new remote port (i.e., like op-code 1).  Future
communications to and from the server will now use the new address family
and the new remote port.

The following are useful tips related to address families:

 - A socket can be associated with only one address family.  For this lab, it
   will be either `AF_INET` (IPv4) or `AF_INET6` (IPv6).  See the man page for
   `socket()`.
 - When using `getaddrinfo()` to create your socket a socket for IPv4 or IPv6
   use, use the `ai_family` member of the `struct addrinfo` variable passed as
   the `hints` argument to `getaddrinfo()`.  For IPv4:
   ```c
   	hints.ai_family = AF_INET;
   ```
   For IPv6:
   ```c
   	hints.ai_family = AF_INET6;
   ```
   Remember that the port passed as the second argument (`service`) to
   `getaddrinfo()` is type `char *`.  Thus, if you only have the port as an
   integer, then you should convert it (not cast it!!).

   See the man page for `getaddrinfo()` for more.
 - The initial communication from the client *must* be over IPv4.

Therefore, every time the client is told to use a new address family (i.e.,
with op-code 4 in a directions response), then the *current socket must be
closed*, and a new one must be created with the new address family.  At this
point, you can also update the value of the variable (e.g., `af`) you might be
using to keep track of the current address family in use.

Note that handling a socket that might be one of two different address families
requires a bit of logical complexity.  That is why previous code snippets
include conditionals such as `if (af == AF_INET)`.  While levels prior to level
4 could use `AF_INET` (IPv4) exclusively, care that you are now handling each
call to `sendto()`, `getaddrinfo()`, `getsockname()`, etc., appropriately for
whatever the current address family is.  That includes the name of the
variables used (e.g., `ipv4addr_remote` vs `ipv6addr_remote`), the address
family applied (e.g., `AF_INET` vs. `AF_INET6`), and more.


### Checkpoint 4

At this point, you can also test your work with
[automated testing](#automated-testing).  Levels 0 through 3 should both work
at this point.

Now would be a good time to save your work, if you haven't already.


# Helps and Hints

## Message Formatting

When writing to or reading from a socket, a buffer must be specified.  This can
be a pointer to data at any memory location, but the most versatile data
structure is an array of `unsigned char`.

For example, in preparing the initial directions request to the server, a
buffer might be declared like this:

```c
unsigned char buf[64];
```

If that initial message has values 1, 3985678983 (0xed90a287), and 7719
(or, equivalently, 0x1e27) for level, user ID, and seed, respectively, the
values stored in that buffer might look like this, after it is populated:

```c
buf = { 0, 1, 0xed, 0x90, 0xa2, 0x87, 0x1e, 0x27 };
```

Of course, you cannot simply populate the array with the above code because the
values will not be known ahead of time.  You will need to assign them.  So how
do you fit an `unsigned short` (16 bits) into multiple slots of `unsigned
char` (8 bits)?  There are several ways.  Consider the following program:

```c
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#define BUFSIZE 8

int main() {
	unsigned char buf[BUFSIZE];
	unsigned short val = 0xabcd;
	int i = 0;
	bzero(buf, BUFSIZE);

	memcpy(&buf[6], &val, 2);
	for (i = 0; i < BUFSIZE; i++) {
		printf("%x ", buf[i]);
	}
	printf("\n");
}
```

When you compile and run this program, you will see that indexes 6 and 7 of
`buf` contain the value of `val`, exactly as they are stored in memory; that is
what the call to `memcpy()` is doing.  You will *most likely* find them to be
in the *reverse* order from what you might expect.  If so, is because the
architecture that you are using is *little endian*.  This is problematic for
network communications, which expect integers to be in *network* byte order
(i.e., *big endian*).  To remedy this, there are functions provided for you by
the system, including `htons()` and `ntohs()` ("host to network short" and
"network to host short").  See their man pages for more information.  Try
modifying the code above to assign `htons(0xabcd)` to `val` instead, and see
how the output changes.

The example above is specific to storing an `unsigned short` integer value into
an arbitrary memory location (in this case an array of `unsigned char`) in
network byte order.  You will need to use this principle to figure out how to
do similar conversions for other cirumstances, including working with integers
other than `unsigned short` and extracting integers of various lengths from
arrays of `unsigned char`.  Hint: the man page for `ntohs()` for related
functions.


## Error Codes

Any error codes sent by the server will be one of the following:

 - 129: The message was sent from an unexpected port (i.e., the source port of
   the packet received by the server).
 - 130: The message was sent to the wrong port (i.e., the remote port of the
   packet received by the server).
 - 131: The message had an incorrect length.
 - 132: The value of the nonce was incorrect.
 - 133: After multiple tries, the server was unable to bind properly to the
   address and port that it had attempted.
 - 134: After multiple tries, the server was unable to detect a remote port on
   which the client could bind.
 - 135: A bad level was sent the server on the initial request, or the first
   byte of the initial request was not zero.
 - 136: A bad user id was sent the server on the initial request, such that a
   username could not be found on the system running the server.
 - 137: An unknown error occurred.


## Op-Codes

The op-codes sent by the server will be one of the following:

 - 0: Communicate with the server as you did previously, i.e., don't change the
   remote or local ports, nor the address family.
 - 1: Communicate with the server using a new remote (server-side) port
   designated by the server.
 - 2: Communicate with the server using a new local (client-side) port
   designated by the server.
 - 3: Same as op-code 0, but instead of sending a nonce that is provided by the
   server, derive the nonce by adding the remote ports associated with the `m`
   communications sent by the server.
 - 4: Communicate with the server using a new address family, IPv4 or
   IPv6--whichever is *not currently* being used.


## UDP Socket Behaviors

For this lab, all communications between client and server are over UDP (type
`SOCK_DGRAM`).  As such, the following are tips for socket creation and
manipulation:

 - Sending every message requires exactly one call to `write()`, `send()`, or
   `sendto()`.  See the man page for `udp`.
 - Receiving every message requires exactly one call to `read()`, `recv()`, or
   `recvfrom()`.  In some cases (e.g., op-code 3) `recvfrom()` *must* be used.
   See the man page for `udp`.
 - When 0 is returned by a call to `read()` or `recv()` on a socket of type
   `SOCK_DGRAM`, it simply means that there was no data/payload in the datagram
   (i.e., it was an "empty envelope").  See "RETURN VALUE" in the `recv()` man
   page.

   Note that this is different than the behavior associated with a socket of
   type `SOCK_STREAM`, in which if `read()` or `recv()` returns 0, it is a
   signal that `close()` has been called on the remote socket, and the
   connection has been shut down.  With UDP (type `SOCK_DGRAM`), there is no
   connection to be shutdown.
 - Generally, either `connect()` must be used to associate a remote address and
   port with the socket, or `sendto()` must be used when sending messages.
   However, for this lab, it is much easier to just use `sendto()` every time
   over using `connect()` because of all the changing ports.
 - `sendto()` can be used to override the remote address and port associated
   with the socket.  See the man page for `udp`.


## Testing Servers

The following domain names and ports correspond to the servers where the games
might be initiated:

 - canada:32400
 - cambodia:32400
 - belgium:32400
 - australia:32400
 - atlanta:32400
 - houston:32400
 - hongkong:32400
 - lasvegas:32400
 - carolina:32400
 - alaska:32400
 - arizona:32400
 - hawaii:32400

Note that communicating with any server should result the same behavior.
However, to balance the load and to avoid servers that might be down for one
reason or another, we have created the following script, which will show
both a status of servers the *primary* machine that *you* should use:

```
$ ./server_status.py
```


# Automated Testing

For your convenience, a script is also provided for automated testing.  This is
not a replacement for manual testing but can be used as a sanity check.  You
can use it by simply running the following:

```
./driver.py server port [level]
```

Replace `server` and `port` with a server and port from the set of
[servers designated for testing](#testing-servers) (i.e., preferably the one
corresponding to your username).

Specifying `level` is optional.  If specified, then it will test
[all seeds](#evaluation) against a given level.  If not specified, it will test
_all_ levels.


# Evaluation

Your score will be computed out of a maximum of 100 points based on the
following distribution:

 - 20 points for each of 5 levels (0 through 4)
 - For each level, 4 points for each seed:
   - 7719
   - 33833
   - 20468
   - 19789
   - 59455


# Submission

Upload `treasure_hunter.c` to the assignment page on LearningSuite.
