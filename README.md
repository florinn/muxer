Muxer
===================

This project is a basic multiplexer/demultiplexer using [Berkeley sockets](https://en.wikipedia.org/wiki/Berkeley_sockets) and [ASN.1](https://en.wikipedia.org/wiki/Abstract_Syntax_Notation_One) encoded messages to connect **transparently** multiple clients to a single server (such that at any time the server is aware of only one client, the muxer).

It may prove useful in a number of scenarios, such as circumventing server limitations for concurrent connections, scalability, security etc.

----------


Overview
-------------

The solution consists of the following projects:

* **common** - any code not directly related to the muxer, like logging, configuration, any generic data structures etc.
* **messages** - the application specific messages exchanged between client and server
* **muxer** - the multiplexer/demultiplexer functionality
* **test_client** - basic client sending uniquely identified messages
* **test_server** - basic server echoing back the message received from test client


How to use it
------------- 
You may compile the code using Visual Studio 2015.

To use the muxer in your project you'd typically need to:

* create ASN.1 specifications for the messages exchanged in your application
* generate encoders/decoders for your messages (replacing the **messages** project) with a tool like [ASN1SCC](https://github.com/ttsiodras/asn1scc)
* implement in `protocol.c` any application level logic (for things like initialization, caching, shutdown) required by your server app to allow the muxer to appear as a single client

> **Note:** The muxer exchanges data in a [Protocol Data Unit (PDU)](https://en.wikipedia.org/wiki/Protocol_data_unit) format as a header followed by payload. The header contains the byte size of the payload while the payload is the ASN.1 encrypted message (to simplify things the message fragmentation was not considered).
