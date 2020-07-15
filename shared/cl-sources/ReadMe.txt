You can obtain cryptlib 3.4.2 sources from Peter Gutmann's site
http://www.cs.auckland.ac.nz/~pgut001/cryptlib/
...or from MacPorts mirrors, for example
http://mirror.lug.udel.edu/pub/macports/distfiles/cryptlib/

I've made few hacks in the code that enchance/modify SSH support.
To compile cryptlib for KpyM Telnet/SSH Server - apply patches from this directory
to cryptlib 3.4.2 sources and build cryptlib with KPYM_HACK macro defined.
