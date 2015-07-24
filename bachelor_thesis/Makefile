all: prace.pdf

# LaTeX je potreba spustit dvakrat, aby spravne spocital odkazy
%.pdf: %.tex $(wildcard *.tex)
	pdflatex $<
	while grep 'Rerun to get ' $*.log ; do pdflatex $< ; done

prace.pdf: prace.acr

%.acr: %.acn
	diff $*.acn $*-copy.acn || makeglossaries $*;
	cp $*.acn $*-copy.acn; done

%.acn: %.tex $(wildcard *.tex)
	pdflatex $<

clean:
	rm -f *.log *.aux *.toc *.out
	rm -f *.acn *.acr *.alg *.glsdefs *.ist
	rm -f prace.pdf

.SECONDARY:
