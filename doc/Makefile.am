EXTRA_DIST = jrtplib.tex

doc: jrtplib.pdf
	
jrtplib.pdf: jrtplib.tex
	pdfelatex jrtplib.tex
	pdfelatex jrtplib.tex

clean:
	rm -f jrtplib.{toc,aux,log,pdf}
