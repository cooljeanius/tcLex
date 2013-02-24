# XML.tcl
#
# This is a working limited XML parser written using the tcLex extension.
# It supports CDATA blocks as well as _nested_ INCLUDE and IGNORE blocks.
# It lacks DTDs and processing instructions handling. It also needs some
# cleanup as well as more conformance with the XML spec (a test suite
# would be great)
#

package require tcLex 1.1

namespace eval ::XML {
  # This namespace contains the lexer as well as several utility procs

  # Basic predefined entities
  # The array Entities associates entities names to their string value
  variable Entities; array set Entities {
    amp  "&"
    lt   "<"
    gt   ">"
    apos "'"
    quot "\""
  }

  # The array EntityStrings is the dual of Entities: it associates
  # actual strings to entities names
  variable EntityStrings; foreach e [array names Entities] {
    set EntityStrings($Entities($e)) $e
  }

  # Utility proc, converts an entity into a string. If the entity
  # doesn't exist, return the string verbatim
  proc getEntityString {entity} {
    variable Entities

    if {[regexp {^&([A-Za-z]+)} $entity all ent] && [info exists Entities($ent)]} {
      return $Entities($ent)
    } else {
      return $entity
    }
  }

  # Escapes the string, ie convert special chars to entities
  proc escape {string} {
    variable EntityStrings
    regsub -all {[\$\\]} $string {\\&} string
    regsub -all {[&<>"']} $string {\&$EntityStrings(&);} string
    return [subst -nocommands $string]
  }
  # Unescape string, convert all entity references to the
  # corresponding chars
  proc unescape {string} {
    regsub -all {[][\\]} $string {\\&} string
    regsub -all {&[A-Za-z]+;?} $string {[getEntityString &]} string
    return [subst -novariables $string]
  }

  # Debugging proc
  proc debug msg {
    puts stderr "XML: $msg\n  (condition stack: [list [[lexer current] conditions]])"
  }

  # The XML parser
  # it generates a flat list containing tag-content pairs. Tags
  # can be open or close tags, or CDATA for plain text.
  lexer parse -args {{debug 0}} \
  -ec {
    comment starttag endtag proc INCLUDE IGNORE CDATA
    attlist attname attval
    stringd stringq
  } \
  -pre {
    # Initialization
    variable Entities

    # variable storing temporary results
    set tag {}     ;# tag name
    set content {} ;# element's text content
    set string {}  ;# string content (enclosed between '' or "")

    # variable holding the result
    set result {}
  } \
  -resultvar result \
  -post {
    # If some content remains, append it as CDATA. This may happen
    # with misformed documents (eg. missing end tags)
    if {$content != {}} {
      lappend result CDATA $content
      if {$debug} {
        debug "misformed document: remaining text \"$content\" (end tag missing?)"
      }
    }
  } {

    {initial INCLUDE IGNORE} "(\n?)<(!--)"                                       {all nl c el} -
    -                        "(\n?)<(!\\[)[ \t\r\n]*([^[ \t\r\n]+)[ \t\r\n]*\\[" {all nl c el} -
    -                        "(\n?)<(\\??/?)([^ \t\r\n/>]*)"                     {all nl c el} {
      # Beginning of tag

      if {$c != "/"} {
        # Ignore newline only before an end tag
        append content $nl
      } 
      # Append pending content as CDATA unless we're in
      # an IGNORE block
      if {$content != {} && [parse conditions -current] != "IGNORE"} {
        # Append non-IGNORE'd text
        lappend result CDATA $content
        set content {}
      }

      # What to do depends on the tag type
      switch -- $c {
        !-- {
          # Comment
          parse begin comment
          set tag "!--"
        }
        !\[ {
          # Marked section

          # TODO: Handle XML case sensitivity options
          set ms [string toupper $el]
          switch -- $ms {
            INCLUDE {
              if {[parse conditions -current] == "IGNORE"} {
                #  If we're in an IGNORE block, also ignore INCLUDE's content
                parse begin IGNORE
              } else {
                parse begin INCLUDE
              }
            }
            IGNORE - CDATA {
              parse begin $ms
            }
            default {
              # Unknown, ignore
              if {$debug} {
                debug "unknown marked section: \"$ms\""
              }
              parse begin IGNORE
            }
          }
        }
        \? - \?/ {
          # processing instructions
          parse begin proc
          set tag $c$el

          # expecting attributes
          parse begin attlist
        }
        / {
          # end tag
          parse begin endtag
          set tag $c$el
        }
        default {
          # start tag
          parse begin starttag
          set tag $el

          # expecting attributes
          parse begin attlist
        }
      }
    }

    {comment} "-->\n?" {} {
      # End of commented section
      parse end
      if {[parse conditions -current] != "IGNORE"} {
        # Append non-IGNORE'd text
        lappend result $tag $content
      }
      set content {}
    } \
    - {[^-]+} {all} -
    - .       {all} {
      # Commented text
      append content $all
    }

    {CDATA INCLUDE IGNORE} "\\]\\]>\n?" {} {
      # End of marked section
      set oldcond [parse conditions -current]
      parse end

      switch $oldcond {
        CDATA {
          if {[parse conditions -current] != "IGNORE"} {
            # Append non-IGNORE'd text
            lappend result CDATA $content
          }
        }
        INCLUDE - IGNORE {
        }
      }
      set content {}
    }
    CDATA {[^]]+} {all} -
    -     .       {all} {
      append content $all
    }

    {initial INCLUDE IGNORE attval stringd stringq} {&([A-Za-z0-9]+|#([0-9]+|x[A-Fa-f0-9]+));?} {text ent} {
      # Entities
      if {[info exists Entities($ent)]} {
        # Only replace existing entities, others are kept verbatim
        set text $Entities($ent)
      }
      
      # Depending on the condition, we append the string at different places
      switch [parse conditions -current] {
        stringd - stringq {
          # We're within a string
          append string $text
        }
        attval {
          # We're expecting an attribute value
          # TODO: do we accept entities outside of strings?
          append attval $text
        }
        default {
          # We're certainly expecting CDATA
          append content $text
        }
      }
    }

    {initial INCLUDE IGNORE} "([^\n&<])+" {all} -
    -                        .            {all} {
      # Any other character is CDATA
      append content $all
    }

    {stringd} {"} {} -
    {stringq} {'} {} {
      # End of string
      parse end
      if {[parse conditions -current] == "attval"} {
	  # We transform white space sequences into single spaces
        # within attribute values
        regsub -all "\[ \t\r\n\]" $string " " string
        lappend content $attname $string
      } else {
        # TODO: improve this. For now we consider single strings
        # as booleans. Eg. <element "attribute"> is like 
        # <element attribute="attribute">. This is due to the lack
        # of special treatment for processing instructions.
        lappend content $string $string
      }
    }
    {stringd} {[^"&]+} {all} -
    {stringq} {[^'&]+} {all} {
      # Any other character is string content
      append string $all
    }

    {attlist attval} {"} {} {
      # Beginning of double-quoted string
      parse begin stringd
      set string ""
    }
    -  {'} {} {
      # Beginning of single-quoted string
      parse begin stringq
      set string ""
    }

    {endtag} {[^>]+} {all} {
      # Ignore endtags remaining characters
      if {$debug} {
        debug "extra characters after endtag: \"$all\""
      }
    }

    {attval}  "/?\\??>\n?" {all} -
    {attlist} "/?\\??>\n?" {all} {
      # End of tag => end of attribute list or value
      parse end

      # Using reject jumps to the next rule. When coming from attval
      # we first jump to attlist (same action but different rule)
      # then to the rule just below. Useful, isn't it ;-)
      parse reject
    }
    {starttag endtag proc} "(/?)(\\??)>(\n?)" {all end pi nl} {
      # End of tag

      set oldcond [parse conditions -current]
      parse end

      if {[parse conditions -current] != "IGNORE"} {
        # Append non-IGNORE'd text
        lappend result $tag $content

        if {$end == "/"} {
          if {$oldcond == "starttag"} {
            # Empty element => append an end tag
            lappend result /$tag {}
          } else {
            # Can't happen (handled by a previous rule) but reported in case of...
            if {$debug} {
              debug "syntax error: empty end tag: \"/$tag/\""
            }
          }
        }
      }

      set content {}
      if {$oldcond == "endtag" || $end == "/"} {
        # Ignore newline only after a start tag
        append content $nl
      } 
    }

    {attlist} "([ \t\r\n]+)" {} {
      # Ignore white space in attributes lists
    }
    -         "([^= \t\r\n>]+)[ \t\r\n]*=[ \t\r\n]*" {all attname} {
      # Attribute name followed by a value
      parse begin attval
      set attval ""
    }
    -         "[^ \t\r\n/>]+" {attname} {
      # Boolean attribute: 
      # TODO: are they allowed in XML?
      lappend content $attname $attname
    }

    {attval} "[ \t\r\n]+" {} {
      # Space ends an attribute's value
      parse end
    }

    * . c {
      # Unrecognized character
      if {$debug} {
        debug "unrecognized character \"$c\""
      }
    }
  }

  # The following procedures work on the parsed list

  # Get the value of an attribute from an attribute list
  proc attributeValue {attlist attname {default {}}} {
    set attname [string toupper $attname]
    foreach {an av} $attlist {
      if {[string toupper $an] == $attname} {
        return $av
      }
    }
    return $default
  }

  # Returns XML text from a list
  proc serialize {list} {
    set result ""

    foreach {el ct} $list {
      if {[string toupper $el] == "CDATA"} {
        append result [escape $ct]
      } elseif {[regexp "^/?(!|\\?)" $el]} {
        append result "<$el"
        if {$ct != ""} {
          append result " $ct"
        }
        append result ">"
      } else {
        append result "<$el"
        foreach {atname atval} $ct {
          append result " $atname=\"[escape $atval]\""
        }
        append result ">"
      }
    }
    return $result
  }
  
  # Transform a XML list into a tree
  # Subtrees are opened at tags other than CDATA and closed
  # at end tags
  proc list2tree {list} {
    set level 0
    foreach {el ct} $list {
      if {[string toupper $el] == "CDATA"} {
        lappend tree($level) CDATA $ct {}
      } elseif {[regexp "^/?(!|\\?)" $el]} {
        # processing instructions
        lappend tree($level) $el $ct {} 
      } else {
        if {[regexp "^/" $el]} {# end tag
          if {$level > 0} {
            incr level -1
          }
         lappend tree($level) $tree([expr {$level+1}])
         } else {# start tag
          lappend tree($level) $el $ct
          incr level
          set tree($level) {}
        }
      }
    }
    while {$level > 0} {
      incr level -1
      lappend tree($level) $tree([expr {$level+1}])
    }
    return $tree(0)
  }

  # Transform a XML tree into a list (the above proc's counterpart)
  proc tree2list {tree} {
    set list {}
    foreach {el ct sub} $tree {
      if {[string toupper $el] == "CDATA" || [regexp "^/?(!|\\?)" $el]} {# no subtree
        lappend list $el $ct
      } else {
        lappend list $el $ct
        eval lappend list [tree2list $sub]
        lappend list /$el {}
      }
    }
    return $list
  }

  # Compare the structures of 2 XML trees
  proc compareTree {t1 t2 indent} {
    set dif ""
    set i 0
    foreach {el1 ct1 sub1} $t1 {el2 ct2 sub2} $t2 {
      set dif2 [compareTree $sub1 $sub2 "  $indent"]
      if {$el1 != $el2 || ($el1 != "CDATA" && $ct1 != $ct2)} {
        append dif "$indent$i: [list $el1] [list $ct1] <=> [list $el2] [list $ct2]\n"
        if {$dif2 != ""} {
          append dif "$dif2"
        }
      } elseif {$dif2 != ""} {
        append dif "$indent$i: [list $el1] [list $ct1]\n$dif2"
      }
      incr i
    }
    return $dif
  }

  # Iterate on specific tags over a XML tree, ignoring other tags
  proc iterate {tags vars tree script} {
    set ltags {}
    foreach tag $tags {
      lappend ltags [string toupper $tag]
    }
    upvar [lindex $vars 0] upel [lindex $vars 1] upat [lindex $vars 2] upct
    if {[lindex $vars 3] != ""} {
      upvar [lindex $vars 3] seq
    }
    if {[lindex $vars 4] != ""} {
      upvar [lindex $vars 4] next
    }
    set seq  "none"
    foreach {el at ct} $tree {
      set el [string toupper $el]
      if {[lsearch $ltags $el] >= 0} {
        set next $el
        if {$seq != "none"} {
          uplevel $script
        }
        set upel $el; set upat $at; set upct $ct
        if {$seq == "none"} {
          set seq "first"
        } else {
          set seq "middle"
        }
      }
    }
    if {$seq == "first"} {
      set seq "sole"
    } elseif {$seq != "none"}  {
      set seq "end"
    }
    set next ""
    if {$seq != "none"} {
      uplevel $script
    }
  }

  # Extract CDATA from a tree
  proc getCDATA {tree} {
    set res ""
    foreach {el at ct} $tree {
      if {[string toupper $el] == "CDATA"} {
        append res $at
      } else {
        append res [getCDATA $ct]
      }
    }
    return $res
  }
}
