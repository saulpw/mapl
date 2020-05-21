test: mapl tests.mapl
	cat tests.mapl | ./mapl

out-tests.txt: mapl tests.mapl
	cat tests.mapl | ./mapl | tee $@
	git diff $@

mapl: main.c mpl.h
	gcc -ggdb -o $@ $<

clean:
	rm -f mapl
