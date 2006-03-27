# Winefish LaTeX Editor
#
# Makefile (generate DOCUMENTATION)
#
# Copyright (C) 2005 Ky Anh <kyanh@o2.pl>

default: html

all: winefish winefish-en

winefish winefish-en:
	@pdflatex $(@)
	@pdflatex $(@)
	@rm -fv $(@).{lg,log,toc,dvi,aux,out,ps}

clean distclean:
	@rm -fv *.{tmp,pdf,log,toc,dvi,aux,out,4ct,4tc,css,html,idv,log,xref,fls}

txt:
	@echo '<HTML><HEAD><STYLE>B{font-weight:normal;text-decoration:underline;color: green;}</STYLE><TITLE>Winefish LaTeX Editor</TITLE></HEAD><BODY><PRE>' > tmp9.html
	@sed \
		-e 's/Winefish Documentation/<h1><a href="http:\/\/winefish.berlios.de\/">Winefish Documentation<\/a><\/h1>/g' \
		-e 's/^% .\+\+//g' \
		-e 's/^\([0-9]\+\). \([:A-Z ]\+\)/<h2>\1. <a name="\1"><a href="#\1">\2<\/a>\&nbsp;\&nbsp;\&nbsp;\&nbsp;<a href="#top">#top<\/a><\/h2>/g' \
		-e 's/^\t\([0-9]\+\). \([:a-z ]\+\)/\&nbsp;\&nbsp;\&nbsp;\&nbsp;\1 <a href="#\1">\2<\/a>/g' \
		-e 's/\(\t\+% .\+\)/<font color="red">\1<\/font>/g' \
		-e 's/\t/\&nbsp;\&nbsp;\&nbsp;/g' \
		-e 's/\[\]/???/g' \
		-e 's/\[/<b>/g' \
		-e 's/\]/<\/b>/g' \
		-e 's/$$Id/<br><br><br>$$Id/g' \
	winefish.txt > tmp1.html
	@sed \
		-e 's/???/\]<\/b>/g' \
	tmp1.html > tmp2.html
	@cat tmp2.html >> tmp9.html
	@echo '</PRE></BODY></HTML>' >> tmp9.html
	@grep -E '.+' tmp9.html > winefish.html
	@rm -f tmp*.htm
html:
	@k2h winefish.txt >winefish.html

