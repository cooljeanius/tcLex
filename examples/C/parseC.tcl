# parseC.tcl
# 
# parseC parses a C file and extracts comments. It returns a list of the form
# "comment <index> <string> ..." where <index> is the line.columnn text-widget-style
# index where # the comment starts and <string> contains the characters between
# /**/ delimiters. The line counting is done as follows: a newlines matching rule,
# active in any condition (ie the conditions list is "*"), is placed at the top
# of the rules. When matched, a line counter is incremented and the rule is
# rejected so other rules can match (the lexer behaves as the rule didn't match).
# Other rules must either:
#   - avoid matching newline characters amongst other chars (in a range
#     for example) and if they need so, match newlines separate from the main
#     regexp (like the last comment rules) because in the contrary they may 
#     silently eat newline chars. The way rules are ordered allows the line
#     counting rule to be called every time a newline is met, whatever the
#     condition
#   - count newline chars in the matched string and increment the line counter
#     in accordance.
#
# parseC only uses one condition named "comment", because it only parses one
# type of pattern. The advantage of using lexer rather than regexp/regsub for
# such simple parsing is the extra processing allowed by lexer, such as line
# and column counting. Anyway, regexp/regsub would require several passes 
# whereas lexer only does one, and lexer can work incrementally on partial
# strings. Moreover, regexp/regsub may fail in tricky cases (such as comments
# containing * chars) unless carefully (and painfully ;-) designed.
# This lexer fails in some cases, when for example comment delimiters are
# inserted in C strings like "this is /* not a */ comment". This is because
# parseC is a rather incomplete C parser, only the comment parsing rules have
# been written. Adding string parsing rules would of course resolve the problem.
#
# The -pre script initializes work variables. See these variables like local
# variables for procs. Even if the lexing is interrupted during an incremental
# processing, the code inside the lexer behaves as it it was run in one pass
# (like a proc), so variables keep their state. This is because lexers use
# their own call frames that, unlike proc, are saved between consecutive
# partial calls instead of being deleted once exited.
#
# The returned result is specified by -resultvar. This is because no extra
# processing has to be done on the local variable "result" before returning,
# even during incremental processing, so taking its value is just fine.
#

package require tcLex 1.1

lexer parseC \
  -ec {comment} \
  -pre {
    # Initializations

    # The variable holding the result
    set result {}

    # Line counting variables
    set lineNb 1
    set lineStart 0
  } \
  -resultvar result \
{
  * "\n" {} {
    # End of line: line counting rule

    # Get line number and line start index
    incr lineNb
    set lineStart [expr {[parseC index]+1}]

    # Reject the rule so that processing may go on
    parseC reject
  }

  {initial} {/\*} {chars} {
    # Comment begin

    # Get line and col number
    set commentLine $lineNb
    set commentCol [expr {[parseC index]-$lineStart}]

    # Initialize string
    set commentString $chars

    # Enter the "comment" condition
    parseC begin comment
  }
  {comment} {\*/} {chars} {
    # Comment end

    # Append chars to the comment string
    append commentString $chars

    # Leave the current condition ("comment")
    parseC end

    # Append comment data to the result
    lappend result "comment" $commentLine.$commentCol $commentString
  }
  {comment} "[^*\n]+" {chars} - 
  -         .         {chars} {
    # Commented chars
    #
    # We could directly use the default pattern (second rule, ".") and not
    # the other rule and the _result_ would be unchanged, but we add this
    # rule for better performance: using the default pattern is very
    # expensive since characters have to be matched one at a time, that
    # means all rules have to be tried for every character. Future versions
    # of tcLex may include default rule optimizations but for now we have
    # to add this sort of rule along with (or instead of) the default rule.
    # Not doing so can result in processing time being doubled or worse
    # (as in the current example).
    #
    # Hopefully these performance-enhancing rules are not too difficult to
    # find, just use a range of all the non-matching characters, like in this
    # case where the first rule matches neither '*' nor newline:
    #  - '*' is used in the comment end delimiter when followed by '/'
    #    and such a sequence is matched by the previous rule;
    #  - '\n' needs to be matched apart so that the line counting rule
    #    can match newlines in the middle of a comment.
    # We then use the second rule with the default pattern (".") in order
    # for every unmatched character to be taken into account. This method
    # is safe and strictly equivalent to using the sole default rule.
    #
    # Rule of thumb: such rules are made up by a negated range of the 
    # characters that can appear as first matched characters in all the
    # previous rules.

    # Append chars to the comment string
    append commentString $chars
  }
}
