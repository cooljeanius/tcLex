*******************************************
tcLex: a lexical analyzer generator for Tcl
*******************************************

tcLex is a lexer (lexical analyzer) generator extension to Tcl. It is inspired 
by Unix and GNU lex and flex, which are "tools for generating programs that 
perform pattern-matching on text". tcLex is very similar to these programs, 
except it uses Tcl philosophy and syntax, whereas the others use their own 
syntax and are used in conjunction with the C language. People used to lex or 
flex should then feel familiar with tcLex. tcLex is a small extension (the 
Windows compiled version is about 20kb, and the source is about 150kb), because 
it extensively uses the Tcl library. However, the current doesn't use Tcl's 
regexp code anymore but a patched version is now included in tcLex, which makes 
it slightly bigger (by a few KB). tcLex should work with Tcl 8.0 and later. 
tcLex will NEVER work with earlier versions, because it uses Tcl 8.0's "object" 
system for performance. The most interesting features are:

 * cross-platform support, thanks to Tcl. Though it has been developped on
   Windows and tested on Windows and Unix only, it should work on other 
   platforms as long as Tcl exists on these platforms. Supported Tcl platforms 
   are Windows 95/NT, Unix (Linux, Solaris...) and Macintosh. Other platforms 
   are VMS, OS/2, NeXTStep, Amiga...
 * unlike lex and flex, which only generate static lexers written in C and
   intended to be compiled, tcLex dynamically generates Tcl commands that can be 
   used like other C commands or Tcl procedures from within Tcl scripts or C 
   programs.
 * it uses Tcl regular expressions. That means you don't have to learn another
   regexp language.
 * it works with Tcl namespaces
 * the generated lexer commands can be used in one pass or incrementally,
   because they maintain state information. That way, several instances of the 
   same lexer (eg a HTML parser) can run at the same time in distinct call 
   frames and maintain distinct states (local variables...). Lexer need not be 
   specially designed in order to be used incrementally, the same lexer can 
   transparently be used in one pass or incrementally. This feature is 
   especially useful when processing text from a file or an Internet socket (Web 
   pages for example), when data is not necessarily available at the beginning 
   of the processing.


VERSION

The current tcLex version is 1.2a1. The suffix "a1" means "alpha 1", meaning 
that this version is the first fature-incomplete release of the future 1.2, 
extending and correcting the previous 1.1. The file changes.txt describes the 
changes made between the first version of tcLex and the current version. 
Although it is alpha software, it brings more bugs corrections than new ones 
;-). In this case, alpha means that many planned features are not yet 
implemented, and documentation may be incomplete. Most of the useful info is in 
the file changes.txt. The file TODO.txt contains planned features that needs to 
be implemented.


WHY TCLEX?

When I decided to write tcLex, I needed an efficient way to parse HTML and CSS 
files, in order to build a Web browser (a long term project of mine). I was 
trying to use the built-in Tcl commands (regexp, regsub...) to achieve that. 
Although a basic HTML parser was quite simple to write, the limitations of the 
classic approach began to appear with CSS.
Basically, I needed to limit the scope of the regexp/regsub commands to specific 
parts of the parsed text, depending on lexical rules.
For example, the classic way of transforming a HTML file into a Tcl list (for 
easier processing) is to replace (with "regsub") the HTML tag delimiters (<>) 
with Tcl list delimiters (Stephen Uhler does it this way in his html_lib), or 
with brackets for further evaluation with "subst" or "eval". The problems begin 
to arise when HTML delimiter characters are used within strings for attribute 
values (for example, "<IMG ALT='=> home'>"), in this case this method doesn't 
work and the generated list is invalid. Besides, parsing misformed files will 
certainly fail and generate an error. Apart from that, performance is not 
guaranteed, especially if you want to respect the standards and handle all the 
error cases, and also because you need several passes.
I then considered writing specific C extensions for parsing these files, using 
lexers written with lex or flex, but the problem is that you need one extension 
for each file type. Besides, these parsers are static and not extensible, even 
though their basics are the same.
I finally decided to write a Tcl extension for writing lexers. It first tries to 
follow the (f)lex philosophy and syntax while adapting them to Tcl. It then uses 
Tcl's command line syntax and regular expressions, and Tcl scripts instead of C 
code. It also build dynamic Tcl commands instead of C files intended to be 
compiled. The idea behind that is to write a "generic lexer" that would be 
scriptable and extensible with Tcl. Thus, only one extension is needed whatever 
the text data type can be.


WHERE TO GET TCLEX

Home Page:
http://www.multimania.com/fbonnet/Tcl/tcLex/index.en.htm

Distribution files: 
 - http://www.multimania.com/fbonnet/pub/tcLex12a1.zip
  (Windows binaries for Tcl8.0.5, Tcl8.1.1 and Tcl8.2)

 - http://www.multimania.com/fbonnet/pub/tcLex1.2a1.tar.gz
  (Windows/Unix sources for Tcl8.0.5, Tcl8.1.1 and Tcl8.2)

 - http://www.multimania.com/fbonnet/pub/tcLex1.2a1.patch
  (patch file for version 1.1.4)


SUPPORT

Since 11/17/1998, tcLex has a dedicated mailing list. The Web site for this list 
is: http://www.eGroups.com/list/tclex .
To subscribe, send a e-mail to the following address: 
tclex-subscribe@egroups.com .
Also, I try to answer all the mails users send me regarding tcLex.


BUILDING THE EXTENSION

If you want to compile tcLex yourself, you must know that it needs the Tcl 
source to compile because it makes use of some internal structures. It will 
compile with Tcl 8.0, 8.1 or 8.2.

* Windows:
Precompiled libraries are available in a separate binary distribution. However, 
you can compile the extension yourself. Go to the "src" directory of the source 
distribution, edit the file  "makefile.vc" for Microsoft Visual C++ (no Borland 
file yet, volunteers :-) and edit the different variables to reflect your own 
installation (compiler, Tcl...).

Next, type on the command line:

	nmake -f makefile.vc

Once the compilation has succeeded, type:

	nmake -f makefile.vc install

And it will copy the necessary files in a subdirectory of the Tcl "lib" dir, so 
that it can be used with "package require tcLex" 


* Unix:
(Thanks to John Ellson <ellson@lucent.com> for these files and instructions)
To build tcLex on Unix systems, type:

      cd src
      chmod u+x configure
	chmod u+x install-sh
      ./configure
      make 
      make install

The configure script will attempt to deduce a $PREFIX from an existing Tcl 
installation. You may still want to use --prefix=... if tclsh is not in your 
$PATH, or if you have multiple tclsh installed.

The generated Makefile uses the file $PREFIX/lib/tclConfig.sh that was left by 
the make of Tcl for most of its configuration parameters.

The Makefile generates a pkgIndex.tcl file that is compatible with Tcl7.6 and 
later.


* MacOS:
There are no makefiles for this platform yet, however compilation should be 
easy, there are only two C files. The only things the source needs are the 
variables TCLEX_VERSION, BUILD_tcLex and USE_TCL_STUBS (if appliable) being 
defined at compile time. You can take a look at the makefile for Windows.


BINARY INSTALLATION

Windows:
Three precompiled libraries are provided in the binary distribution, named 
tcLex80.dll, tcLex81.dll and tcLex82.dll, respectively for Tcl 8.0, 8.1 and 8.2. 
Just copy them and the file pkgIndex.tcl in a sub-directory of your choice in 
the Tcl "lib" dir.

MacOS, Unix:
No binary distribution for now.


DOCUMENTATION

The directory doc contains tcLex documentation in HTML files. The documentation
is available in english (subdir en) and french (subdir fr). Read it carefully.


CONTACT, COMMENTS, BUGS, etc.

* Please read the license (file license.txt), and especially the "beerware
  clause" ;-)
* Please send any bugs or comments to <frederic.bonnet@ciril.fr>. Bug reports
  and user feedback are the only way I intend to improve and correct tcLex. If
  no one uses tcLex, I see no reason why I should improve this extension except
  for my own use (for which tcLex is more than adequate for now).
* Even if you have no comment, I'd appreciate that every tcLex user send me a
  mail to the address mentioned above. That gives me information about the
  number of users which is an important part of my motivation. I won't use your
  address to send you unsollicited email, spam, or sell it to telemarketers, but
  only to keep track of users.
* If you find a bug, a short piece of Tcl that exercises it would be very
  useful, or even better, compile with debugging and specify where it crashed in 
that short piece of Tcl.


TO CONTRIBUTE

Since I only use MS Visual C++ on Windows, I'd appreciate contributions from 
people providing other compilers' makefiles (eg Borland), GNU autoconf's, or 
even better, compiled extensions for binay distribution on Macintosh and Unix 
(esp. Linux and Solaris).

I'd also like to gather all the lexers made with tcLex in a single place (a Web 
page for example) as a valuable resource for the Tcl community. Interesting 
contributions would be for example parsers for HTML, XML, CSS, JavaScript and 
other Web languages.

