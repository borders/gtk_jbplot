all: test/test1 test/dp

test/test1: jbplot.c jbplot.h test/test1.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o test/test1 jbplot.c test/test1.c jbplot-marshallers.c \
		`pkg-config --libs --cflags gtk+-2.0`

test/dp: jbplot.c jbplot.h test/dp.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o test/dp jbplot.c test/dp.c jbplot-marshallers.c \
		`pkg-config --libs --cflags gtk+-2.0`

jbplot-marshallers.c: jbplot-marshallers.list
	glib-genmarshal --prefix _plot_marshal --body $< > $@

jbplot-marshallers.h: jbplot-marshallers.list
	glib-genmarshal --prefix _plot_marshal --header $< > $@

.PHONY: docs
docs: jbplot.c jbplot.h
	@ echo "Generating documentation..."
	@ doxygen Doxyfile > /dev/null

clean:
	rm -f jbplot-marshallers.h jbplot-marshallers.c
	rm -f test/test1
