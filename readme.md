Licence
=======

MAF is free software, provided without warranty of any kind under the terms of the GNU Public Licence v3.
For detailed licence information refer to the file COPYING.
For information on how to use MAF refer to the help included with the package, which is also available [online](http://maffsa.sourceforge.net/manpages/MAF.html).

Latest changes
==============

2.1.1 A bug in the code used for FSA minimisation has been fixed. The bug caused composite multipliers for coset systems to be generated incorrectly under some circumstances. The most likely effect of this was that axiom checking would erroneously fail, or that automata, gpsubpres, or gpsublowindex could fail while generating a subgroup presentation. A minor
change has been made to gpmult. The operation of the -sub option has been slightly changed. Previously inverse words were added for each specified subgroup generator. This is no longer
done.

2.1.0 There are two new utilities. The more important one is gpxlatwa, which allows a word-acceptor to be translated into a word-acceptor for another alphabet. There is also a fix to the parsing of expressions in input files. Previously an invalid equation such as [a\*b)^5,IdWord] would have been parsed without any warning as [a\*b,IdWord], because a closing parenthesis without a matching opening parenthesis was treated as "end of expression". There is also a minor fix to the fsaprune utility. Previously the documented -accept_all option was not in fact accepted.

For earlier fixes refer to [Release Notes](http://maffsa.sourceforge.net/changes.html)

Installation
============

Windows
-------

As yet there is no install program for MAF. Simply unzip all the files into the directory where you would like MAF to be installed. For example c:\maf or C:\Program Files\MAF. As yet MAF can only be used from the command line. You will probably find it helpful to set up a batch file that opens your chosen command shell (e.g. cmd.exe or cygwin bash) at a convenient location and which adds the bin directory for MAF to the path.

For more information about MAF refer to the documentation in the help subdirectory or visit the MAF project on Sourceforge.net <https://sourceforge.net/projects/maffsa/>

Mac
---

Refer to Macnote.txt