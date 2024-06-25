This directory contains a variety of input files for the group 6561#8, one of
14 presentations thought to be for groups of order 6561, discussed in the paper "Groups of Deficiency Zero".
All three generators have order 27, though this is not at all obvious.

MAF can find a confluent rewriting system quickly for some word orderings but
not for others. The time to find the system varies widely, from 1 minute or
under to possibly many hours. The times are incredibly sensitive: the
time might be faster on a slower computer, and can vary by several orders
of magnitude after apparently innocuous changes to the code of MAF.

With some word-orderings various unpleasant things happen. For example with the file 6561#8_aAcCbB_recursive the system quickly gets into a state where many words cannot be reduced at
all due to word-length being exceeded. With the file 6561#8_bBaAcC_rt_recursive the system gets into a state where word-reduction takes an inordinately long time. In that case even when MAF knows which exactly which words are reducible, finding the correct RHS for all the primary equations takes a very long time.

With the addition of a^27=Idword, which is true but not easily proved,
all the examples become easy.
