/* Project  : tcLex examples
 * File     : strip.l
 * Language : C, with Flex
 * Version  : 0.1
 * Dates    : 16/11/1998
 * Source   : flex man page
 *
 * Trivial white-space stripping program
 *
 * To pre-process, compile and run this program:
 *    flex -ostrip.lex.yy.c strip.l     generates scanner
 *    gcc -o strip strip.lex.yy.c -lfl  compiles scanner
 *    ./strip < textfile                runs scanner
 */


%%
[ \t]+  putchar(' ');
[ \t]+$ /* ignore this token */
