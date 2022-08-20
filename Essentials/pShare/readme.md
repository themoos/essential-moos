# Bridging MOOS Communities with pShare


## Contents

- 1 Introduction
   - 1.1 Why UDP?
- 2 Basic Operation
   - 2.1 Different Ways Sharing
   - 2.2 Sharing via Multicast Channels
- 3 The form of command line configuration strings
   - 3.1 Output
   - 3.2 Input
- 4 CommandlineConfiguration
- 5 ConfiguringpSharefroma.moosfile
- 6 Wildcard Sharing
   - 6.1 Caret Sharing: A Special Case of Wildcard Sharing
- 7 Instigating Dynamic Shares On The Fly


## 1 Introduction

MOOS-V10 brings with it a new command line application which allows data to be shared between MOOS communities. Recall that in MOOS parlance a“community”is the set of programs talking to a particular instance of a
MOOSDB including the MOOSDB itself. In some ways pShare is just a modern, better written version of pMOOSBridge in others it offers much greater flexibility, and functionality. Here is a quick summary of pShare functionality

- it offers UDP communication between communities
- it can share data over multicast channels
- it allows renaming of variables as they are shared
- it supports wildcard sharing - so you can specify a sinple regular expression
    for which what should be shared
- it supports dynamic configuration (via MOOS) of sharing/forwarding/re-
    naming
- it supports command line configuration and from a .moos file
- it can be completely configured from the command line

It is worth, straight offthe bat, understanding how in usage terms, pShare is
different from pMOOSBridge

- pShare, unlike pMOOSBridge, only supports UDP (or multicast which is a
    kind of UDP)
- You need one instance of pShare per community (compare this to pMOOSBridge
    where a single instance could be used to bridge any number of communi-
    ties)
- Currently, pShare only supports sharing of up to 64K messages

### 1.1 Why UDP?

UDP is, of course, a lossy affair - there is no guarrantee that messages will get through and indeedpShareis intended for use in just such situations. Use pSharewhen you want, when possible, to get messages between communities and yet you don’t mind dropping a few messages. Perhaps this applies when you have a deployed robot out in the wilds and the wireless link simply doesn’t support tcp/ip as well as you might hope. If you do mind loosing things then you must use tcp/ip (standard MOOS) and if you have
a lossy connection you will spend years waiting for data.

[ToDo: insert diagram]

Figure 1: A simple use of pShare: two communities are linked by two instances of pShare - one in each community.

## 2 Basic Operation

Figure 1 shows a typical and simple use case ofpShare. Here two communities are linked by two instances ofpShare. Each is connected to a MOOSDB and each pShare is configured (by a means we will get to in a minute) to subscribe to messages from theMOOSDB(issued by clients). These messages are forwarded over a udp link to the other pShare instance which inserts them into it’s own MOOSDB. The important point here is that if process “A” in community “P” has message M shared via pShareP and pShareQ to process B in community Q,then when B receives M it will still have A as its source and P as its source community. So to process B it looks like A is actutaly in its own community (Q).

A more complicated (marginally) example is shown in Figure2. Here the left hand community is sharing as an output data to the top right and bottom right communities but only receiving data from the bottom right.

### 2.1 Different Ways Sharing

Each instance ofpSharecan be configured such that it can

- forward a named message (like ’X’) to any number of specific udp ports
    on any number of other machines
- can rename a message before forwarding
- receive and forward on to its own MOOSDB messages from any number of
    otherpShares
- forward messages on predefined or any number of multicast channels
- receive messages on any number of multicast channels

how to do this is best explained with some examples and that will happen in Section 4. Before that it is worth explaining the merit of multicast channels.


```
pShare
```
```
pShare
```
```
MOOSDB
```
```
MOOSDB
```
```
pShare
```
```
MOOSDB
```
Figure 2: A simple use ofpShare: three communities are linked by two instances of pShare - one in each community but data sharing is not symmetric.

### 2.2 Sharing via Multicast Channels

Imagine you as an application developer knew that other communities (but you do not know which ones a-priori) would be interested in a variable called X.

Now, if you knew exactly who wanted it you could configure a standard UDP shares to mutually agreed ports (presumably one port per community) on which otherpSharesare listening. But you don’t. So what to do? Well you could use pShare’s ability for packets to predefine multicast channels ( these are really simply multicast addresses behind the scenes) you can tell pShare to forward MOOS messages out to a multicast_channel and later on any number of other pShareinstances can subscribe to this channel and receive them.

## The form of command line configuration strings

### Output

The ’-o’ switch allows you to configure which messages to forward (share), how to rename them and where to send them. At its highest level the the ’-o’ switch is followed by a comma separated list of mappings.  `-o= mapping , mapping, mapping,...` and each mapping describes how one MOOS variable is routed to any number of destinations.

A single mapping contains a direction of a variable name to one or more routes . A mapping has the syntax
```
var_name->route & route & route....
```
where a route has the form
```
new_name: destination
```
and destination is one of

* an address and port pair "address:port"
* a multicast channel as described later.

### 3.2 Input

The -i switch is much simpler. It tells an instance of pShare how to listen to for incoming traffic. The format is always -i=localhost:<port_num> or i=multicast_<N> where N is a number between 0 and 255. Multiple listens
can be specified in a comma separated list.

## CommandlineConfiguration

Imagine we have two communities A and B. Lets also assume that they reside on different machines. Machine A has ip address 192.168.0.10 and machine B has ip address 192.168.0.4.

```
// share X from A to B
terminal A command line -o=’X->192.168.0.4:10000’
terminal B command line -i=localhost:
```

```
// share X from A to B as Y
terminal A command line : pShare -o=’X->Y:192.168.0.4:10000’
terminal B command line : pShare -i=localhost:
```
```
// share X from A to B as X and Y
terminal A command line : pshare -o=’X->92.168.0.4:10000 & Y:192.168.0.4:10000’
terminal B command line : pshare -i=localhost:
```
```
//share X from A to B as X and Y via two different ports
terminal A command line : -o=’X->92.168.0.4:10000 & Y:192.168.0.4:20000’
terminal B command line : -i=localhost:10000,localhost:
```
```
// share X and Y to B
terminal A command line : pshare -o=’X->192.168.0.4:10000 , Y->192.168.0.4:10000’
terminal B command line : pshare -i=localhost:
```
```
// share X via multicast
terminal A command line : pshare -o=’X->multicast_7’
terminal B command line : pshare -i=multicast_
```
```
// share X via multicast and rename
terminal A command line : pshare -o=’X->Y:multicast_7’
terminal B command line : pshare -i=multicast_
```
```
// share X on several channels
terminal A command line : pshare -o=’X->Y:multicast_7 & Z:multicast_3’
terminal B command line : pshare -i=multicast_
```
```
// share X via multicast and rename
terminal A command line : pshare -o=’X->Y:multicast_7’
terminal B command line : pshare -i=multicast_7,multicast_
```
```
// share X as several new variables on the same multicast channel
terminal A command line : pshare -o=’X->Y:multicast_7 & Z:multicast_7’
terminal B command line : pshare -i=multicast_
```
Tip:don’t forget to put single quotes around the routing directives to prevent your shell from interpretting the ’>’ character.


## Configuring pShare from a moosfile

We have seen some examples on how to configure pShareon the command line (because that is insanely useful) but of course it can also be configured by reading a configuration block in a .moos file just like anyMOOSAppcan. The key parameter names are `Output` which can have the same format as the -o flag on the command line
or a more verbose form as illustrated below. There can be as many “Output” directives in a configuration block as you need. The verbose form specifies one share per invocation while the compact form specifies as many as you
wish. The verbose form of the Output directive is a tuple of token value pairs where the tokens are

* `src_name` the name of the varible ot be shared
* `dest_name` the name it should have when it arrives at its destination - this is optional, if it is not present then no renaming occurs
* `route` a description of the route which could be for udp shares host `name:port:udp` or, for multicast shares,  `multicast_X`. This is much as it is for the command line configuration.
*Input which can have the same dense format as the -i flag on the command line as described above or a more verbose, intuitive form illustrated below. In the long hand version you use a single token value pair with a token name of “route” as described above. This specifies the fashion in which this instance of pShare should listen - be that on mulitple ports for udp traffic or on a multicast channel for multicast action.
   
   
Here is an example configuration for pShare which you would find in a `.moos` file   
   
```
ProcessConfig=pShare
{
   //a verbose way of sharing X, calling itYand sending
   //on mulitcast_
   Output=src_name=X,dest_name=Z,route=multicast_

   //a verboseway of sharing Y calling itYYand sending
   //it to port 9832 on this machine
   Output=src_name=Y, dest_name=YY, route=192.6.8.3:

   //a verbose way of sharing T, sending it withoutnamechange
   //to port 9832 on a remote machine
   Output=src_name=T, dest_name=TT, route=192.3.4.5:

   //a dense specificationwhich sendsXto port 10000 via
   //udp on a remote machine 
   Output=X>192.168.0.4:
   // ...and Y to a different machine while renaming it to ’T’
   Output=Y>T:192.168.0.5:

   //specify inwhat placeswe wish to listen to receive
   //the output of other instances of pShare
   //we can do this one at a time using the route directive
   Input=route=multicast_

   //or,we can specifiy multiple routes atonce. Note that
   //we have to usean& character to separate different routes
   //or it lookslike a list ofmalformedtoken value pairs
   
   Input=route=multicast_21&localhost:9833&multicast_

   // we can of course also use wildcards this is where it gets -
   interesting
   //lets share any varialble in community which is 2 characters long and begins
   //with X
   Output=src_name=X?,route= localhost:
   
   //we could be more specific and say we only want to share such variables from
   //a namedprocess. So herewe say only share two letter - variables beginning with Q
   //from a process called procA
   Output=src_name=Q?:procA, route=localhost:
}
```
   
## 6 Wildcard Sharing

It won’t have escaped your attention that MOOS-V10 offers support for wildcarding -that is specifying a pattern which represents a whole set of named variables. (So for example ‘*’ means all variables because the regular expressions character ‘*’ matches all sets of characters). `pShare` can utilise this functionality to make sharing many variables trivial. You can also specify to only share variables from a specific process.

So lets start with a command line example. We can share all variables in a community like this:

```
// share all variables onto channel 7
terminal A command line : pShare -o=’*->multicast_7’
terminal B command line " pShare -i=multicast_
```
And we can be a little more precise and only forward variables which begin

with the letters “SP”


```
description share all variables onto channel 7 which begin with “SP”
terminal A command line -o=’SP*->multicast_7’
terminal B command line -i=multicast_
```
or which begin with “K” end with “X” followed by any single character

```
description starting with X ending with a K plus 1 character
terminal A command line -o=’X*K?->multicast_7’
terminal B command line -i=multicast_
```
We can also be explict about which processes we want to forward from. So

for example say we just wanted to forward messages from teh process called

“GPS”:

```
description share all variables from “GPS” onto channel 7
terminal A command line -o=’*:GPS->multicast_7’
terminal B command line -i=multicast_
```
And of course the process name also supports wild cards so we we can do

```
// var ending in “time” from a proc starting “camera_”
terminal A command line -o=’*time:camera_*->multicast_7’
terminal B command line -i=multicast_
```
A good question is what does it mean to rename a wildcard share? Well that simply serves as suffix to the shared variable name

```
// share all variables onto channel 7 with renaming
terminal A command line -o=’*->T:multicast_7’
terminal B command line -i=multicast_
```
which means a variable “X” will be shared as “TX” - the parameter T is acting as suffix. Similarly a variable called “donkey” would endup being shared in this example as “Tdonkey”.


Finally of course wildcard shares can be specified in configuration files as

shown below.

```
Configuring pShare from a configuration block
ProcessConfig=pShare
{
//we can of course also use wildcards this is where it gets interesting. Lets share any varialble incommunity which is 2 characters long and begins withX
Output= src_name=X?,route=localhost:
```
```
//wecould bemore specificand sayweonlywant to share -
such variables from
//a namedprocess. So herewe say onlysharetwo letter -
variables beginning withQ
//form a process called procA
Output= src_name=Q?:procA, route=localhost:
```
```
//wecan be more general and sendany variable beginning with -
Wfomra process
//whos nameends inAto multicast channel 7
Output=src_name=W⇤:⇤A, route= multicast_
}
```
### Caret Sharing: A Special Case of Wildcard Sharing

pShare supports a special case of wildcard sharing in that it can forward a variable under a new name where the new name is derived from the part of the original name which matches a ’*’ wildcard charcter. Granted this sounds complicated. Its easiest to understand this with a few examples. The important point to remember though is the syntax for this special case it is*<str>->^or <str>*->^where <str> is any string that does not contain a * or ?.

```
// A_X gets shared as A on multicast 7
terminal A command line -o=’*_X->^:multicast_7’
terminal B command line -i=multicast_
```
Note in that in the kind of sharing the ’^’ means the part of the variable name which matches the single ’*’ wildcard character on the src filter. This wildcard character can only occur at the beginning or teh end of the variable pattern. So we can also have:


```
// A_X gets shared as X on multicast 7
terminal A command line -o=’A_*->^:multicast_7’
terminal B command line -i=multicast_
```
   
## Instigating Dynamic Shares On The Fly

pShare can be told to start sharing data dynamically by any MOOS Process simply by publishing a correctly formatted string. The format is simple - its is pretty much the same as a line in a configuration file. You need to write a string“cmd = <directive>” to the variablePSHARE_CMDwhere<directive> is a output or input directive such as you would write in a configuration file.

Here are some examples:

- “cmd = Output , src_name = X?, route = localhost:9021”
- “ cmd = Output , src_name =T, dest_name = TT, route=192.3.4.5:9832”

The ability to dynamically instigate shares turns out to be very useful if you don’t know what needs to be shared whenpSharefirst starts and that only gets figured out by other processes.


