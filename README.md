Granulator
==========

Basic granulation algorithm

Written by Tucker Vento

Command line granulation processing<br/>
Outputs a file called processed.wav

USAGE
-----
<b>first parameter:</b> filename (including .wav extension)<br/>
<b>second parameter:</b> grain write #: how many times to write each grain<br/>
<b>third parameter:</b> grain size: in whole milliseconds<br/>
<b>fourth parameter:</b> attack time: 0-100, as a percent of the grain time<br/>
<b>fifth parameter:</b> reverse grains: 1 or 0, reverses each grain in place, but plays forward (not working)<br/>
<b>sixth parameter:</b> seek thru: 1 or 0, when used in conjunction with grain-repeating it will seek the length of data written per grain, and pull the next grain from that distance away in the original track.  the new track will be the same length as the original<br/><br/>
<b>seventh parameter:</b> loop size: 1+, 1 gives no loop (IN TESTING BRANCH)<br/>
<b>eighth parameter:</b> loop write count: like grain #, for loops  (IN TESTING BRANCH)<br/>
