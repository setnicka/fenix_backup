all: FenixBackup_obhajoba.pdf

%.pdf %.aux %.idx: %.tex
	pdflatex $<
	while grep 'Rerun to get ' $*.log ; do pdflatex $< ; done

clean:
	rm -f *.pdf *.out *.nav *.log *.snm *.toc *.aux *.vrb *.pdfpc

.PHONY: all clean

