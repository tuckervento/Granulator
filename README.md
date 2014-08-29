Granulator
==========

Basic granulation algorithm

Written by Tucker Vento

Command line granulation processing
Outputs a file called processed.wav

first parameter: filename (including .wav extension)
second parameter: grain write #: how many times to write each grain
third parameter: grain size: in whole milliseconds
fourth parameter: attack time: 0-100, as a percent of the grain time
fifth parameter: reverse grains: 1 or 0, reverses each grain in place, but plays forward (not working)
sixth parameter: seek thru: 1 or 0, when used in conjunction with grain-repeating it will seek the length of data written per grain,
    and pull the next grain from that distance away in the original track.  the new track will be the same length as the original
seventh parameter: loop size: 1+, 1 gives no loop (IN TESTING BRANCH)
eighth parameter: loop write count: like grain #, for loops  (IN TESTING BRANCH)
