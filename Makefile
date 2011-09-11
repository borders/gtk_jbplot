all: test/test1 test/dp test/bab test/data_view test/chaos test/newton_cradle test/zing test/set_data

test/zing: jbplot.c jbplot.h test/zing.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o test/zing jbplot.c test/zing.c jbplot-marshallers.c \
		`pkg-config --libs --cflags gtk+-2.0`

test/test1: jbplot.c jbplot.h test/test1.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o test/test1 jbplot.c test/test1.c jbplot-marshallers.c \
		`pkg-config --libs --cflags gtk+-2.0`

test/chaos: jbplot.c jbplot.h test/chaos.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o test/chaos jbplot.c test/chaos.c jbplot-marshallers.c \
		`pkg-config --libs --cflags gtk+-2.0`

test/set_data: jbplot.c jbplot.h test/set_data.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o test/set_data jbplot.c test/set_data.c jbplot-marshallers.c \
		`pkg-config --libs --cflags gtk+-2.0`

test/newton_cradle: jbplot.c jbplot.h test/newton_cradle.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o test/newton_cradle jbplot.c test/newton_cradle.c jbplot-marshallers.c \
		`pkg-config --libs --cflags gtk+-2.0` -lgsl -lgslcblas

test/dp: jbplot.c jbplot.h test/dp.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o test/dp jbplot.c test/dp.c jbplot-marshallers.c \
		`pkg-config --libs --cflags gtk+-2.0` -lgsl -lgslcblas

test/vibe: jbplot.c jbplot.h test/vibe.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o test/vibe jbplot.c test/vibe.c jbplot-marshallers.c \
		`pkg-config --libs --cflags gtk+-2.0`

test/bab: jbplot.c jbplot.h test/bab.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o test/bab jbplot.c test/bab.c jbplot-marshallers.c \
		`pkg-config --libs --cflags gtk+-2.0`


test/data_view: jbplot.c jbplot.h test/data_view.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o test/data_view jbplot.c test/data_view.c jbplot-marshallers.c \
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
	rm -f *.o
	rm -f jbplot-marshallers.h jbplot-marshallers.c
	rm -f test/test1
	rm -f test/chaos
	rm -f test/set_data
	rm -f test/data_view
	rm -f test/dp
	rm -f test/zing
	rm -f test/newton_cradle
