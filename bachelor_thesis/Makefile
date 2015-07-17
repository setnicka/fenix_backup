all: prace.pdf

# LaTeX je potreba spustit dvakrat, aby spravne spocital odkazy
%.pdf: %.tex $(wildcard *.tex)
	pdfcslatex $<
	while grep 'Rerun to get ' $*.log ; do pdfcslatex $< ; done

clean:
	rm -f *.log *.aux *.toc *.out
	rm -f prace.pdf
