#!/usr/local/bin/tclsh8.0
#
# Project  : tcLex examples
# File     : pascal.tcl
# Language : Tcl, with tcLex
# Version  : 0.1
# Dates    : 10/11/1998
# Author   : Neil Walker
#
# "scanner for a toy Pascal-like langauge"
#
# To run this program:
#    ./pascal.tcl textfile
#

package require tcLex 1.1

lexer pascal -longest -nocase {
    {} {[0-9]+}             {integer}   {puts "An integer: $integer"}

    {} {[0-9]+\.[0-9]*}     {float}     {puts "A float: $float"}

    {} {if|then|begin|end|procedure|function} {keyword} {puts "A keyword: $keyword"}

    {} {[a-z][a-z0-9]*}   {id}    {puts "An identifier: $id"}

    {} {\+|-|\*|/}          {op}        {puts "An operator: $op"}

    {} "\{[^\}\n]*\}"       {}          {# eat up one-line comments}

    {} "[ \t\n]+"           {}          {# eat up whitespace}

    {} {.}                  {c}         {puts "Unrecognized character: $c"}
}

if {$argc > 0} {
    pascal eval [read [open [lindex $argv 0] r]]
} else {
    pascal eval [read stdin]
}
