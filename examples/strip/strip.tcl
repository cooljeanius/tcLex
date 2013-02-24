#!/usr/local/bin/tclsh8.0
#
# Project  : tcLex examples
# File     : strip.tcl
# Language : Tcl, with tcLex
# Version  : 0.1
# Dates    : 16/11/1998
# Author   : Neil Walker
#
# Trivial white-space stripping program - to show different lexer styles
#
# Style 1: "all-rules-in-one-list"
# Style 2: "rules-as-arguments"
# Style 3: programmatical method
#
# ALL produce the same output, so choosing between the styles is a
# matter of taste.
#
# To run this program:
#    ./strip.tcl < textfile
#
# Note from Frédéric BONNET:
# This lexer has the same behavior as the one written using flex in
# file strip.l. To do so, it takes its input from the standard input
# and outputs the result to the standard output. If you compare
# both lexers, you will notice that the tcLex version needs an extra
# rule (the default rule) to handle unmatched characters.

package require tcLex 1.1

# Style 1: "all-rules-in-one-list"
# --------------------------------
# no variable substitution (but backslash-characters such as \n\t are
# available), but no quoting hell.  No comments outside the actions
# scripts.

lexer strip1 -lines {
  {} "[ \t]+$"    {}  {# ignore white space at end-of-line}
  {} "[ \t]+"     {}  {puts -nonewline " "; # reduce all spaces to one}
  {} .|\n         {c} {puts -nonewline $c; # echo everything else}
}

# Style 2: "rules-as-arguments"
# -----------------------------
# variable substitution, but much more quoting, and with backslashes at
# each end of line. No comments outside the actions scripts. Apart from
# extra quoting, this style is the one that most resembles flex.

# this would be a macro in flex - here it is a normal Tcl variable
set whitespace "\[ \t\]+"

lexer strip2 -lines \
    {} $whitespace\$    {}  {# ignore white space at end-of-line} \
    {} $whitespace      {}  {puts -nonewline " "; # reduce all spaces to one} \
    {} .|\n                {c} {puts -nonewline $c}


# Style 3: programmatical method
# ------------------------------
# very free-form, very powerful, very Tcl.

# initialise "macro" and empty ruleset
set whitespace "\[ \t\]+"
set rules {}

# rule 1: ignore whitespace at the end of a line 
lappend rules {} $whitespace\$  {}  {}

# rule 2: reduce all other spaces to one space
lappend rules {} $whitespace    {}  {puts -nonewline " "}

# rule 3: echo any other character (flex does this automatically)
lappend rules {} .|\n           {c} {puts -nonewline $c}

# create lexer
lexer strip3 -lines $rules


# pick up file
set string [read stdin]

# now test for each type of lexer - SHOULD give the same result for all!
puts "-------------------------------------------------------"
puts "Type 1:"
puts "-------------------------------------------------------"
strip1 eval $string

puts "-------------------------------------------------------"
puts "Type 2:"
puts "-------------------------------------------------------"
strip2 eval $string

puts "-------------------------------------------------------"
puts "Type 3:"
puts "-------------------------------------------------------"
strip3 eval $string
