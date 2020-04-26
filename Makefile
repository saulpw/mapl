test: mapl tests.mapl
	cat tests.mapl | ./mapl

test-golden.txt: mapl tests.mapl
	cat tests.mapl | ./mapl | tee $@
	git diff $@

mapl: main.c
	gcc -ggdb -o $@ $<

clean:
	rm -f mapl
