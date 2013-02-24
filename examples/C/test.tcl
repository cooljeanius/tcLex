# first source the C parser
source parseC.tcl

# Then read the test.c file
set f [open "test.c"]
set src [read $f]
close $f

# Get the parsed list from the source
set parsedList [parseC eval $src]

# Build a Tk text for the output
pack [text .t] -expand 1 -fill both

# Set the colors of the different sections (ie tags)
# Default text is Courier 10 black on white
.t configure -background white -foreground black -font {Courier 10}
# Here we define comments to be Courier 10 italic black on gray
.t tag configure comment -background gray -foreground black -font {Courier 10 italic}

# Insert the source into the text widget
.t insert 1.0 $src

# Then marks the recognized sections (eg. comments)
foreach {type index text} $parsedList {
  .t tag add $type $index ${index}+[string length $text]chars
}
