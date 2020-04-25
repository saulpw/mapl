test: mapl
	cat tests.mapl | ./mapl
	git diff tests-golden.txt

mapl: main.c
	gcc -ggdb -o $@ $<

clean:
	rm mapl
