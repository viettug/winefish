#ifndef __CONFIG_SPEC_H_
#define __CONFIG_SPEC_H_

/*********************** winefish ***********************
 *
 * outputbox
 * these lines alter `rcfile.c::rc_parse_main()'
 *
 *********************** winefish ***********************/

#define OB_LaTeX "latex -file-line-error-style -src-specials '%B'"
#define OB_PDFLaTeX "pdflatex -file-line-error-style -src-specials '%B'"
#define OB_DVIPS "dvips -o '%B.ps' '%B.dvi'"
#define OB_DVIPDFM "dvipdfm -o '%B.pdf' '%B.dvi'"

#define OB_ViewLog "cat '%B.log'"
#define OB_SoftClean "rm -fv '%B.log' '%B.aux' '%B.toc'"

#define OB_DVI_Viewer "xdvi -editor \"winefish -n0 -l%%l '%%f'\" -sourceposition %l%b.tex %B.dvi &"
#define OB_PDF_Viewer "xpdf %B.pdf &"
#define OB_EPS_Viewer "gv %B.ps &"

#define OB_Dos2Unix "cat '%f' | dos2unix > '%o'"
#define OB_Tidy "cat '%f' | tidy -utf8 -q >'%o' 2>/dev/null"

/*********************** winefish ***********************
 *
 * below things you may modify...
 *
 *********************** winefish ***********************/

/* maximum depth for `find' function */
#define FUNC_GREP_RECURSIVE_MAX_DEPTH "-maxdepth 50"

/* maximum length of latex command (\foobar) */
#define COMMAND_MAX_LENGTH 20

/* maximum length of autotext command (/foobar) */
#define AUTOTEXT_MAX_LENGTH 20

/* delimiters for recognization of latex commands */
#define DELIMITERS " `1234567890-=~!@#$%^&*()_+[]{};':\",./<>?\\|"

/*********************** winefish ***********************
 * The maximum of lines for `brace_finder' when it starts job *AUTOMATICALLY*.
 * Each times `brace_finder' enters a new line, the value of `line-index' will be
 * increased. If `line-index' reaches BRACE_FINDER_MAX_LINES,
 * it returns BR_RET_NOT_FOUND (see brace_finder.c::brace_finder())
 *
 * too big value will slowdown winefish.
 * the best value is the maximum of lines in the visible area...
 *********************** winefish ***********************/

#define BRACE_FINDER_MAX_LINES 30

/* 30*80 */
#define BRACE_FINDER_MAX_CHARS 2400

/*********************** winefish ***********************/

#endif /* __CONFIG_SPEC_H_ */
