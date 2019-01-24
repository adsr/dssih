prefix?=/usr/local

dssih_cflags:=-std=c99 -Wall -Wextra -pedantic -g -O0 -D_GNU_SOURCE -I. $(CFLAGS)
dssih_ldflags:=$(LDFLAGS)
dssih_dynamic_libs:=-lsoundio
dssih_include_dirs:=-Ivendor/soundio -Ivendor/dssi/dssi -Ivendor/ladspa/src -Ivendor/uthash/src
dssih_static_libs:=vendor/soundio/build/libsoundio.a
dssih_ldlibs:=-lasound -lpulse -lm -lpthread -ldl $(LDLIBS)
dssih_objects:=$(patsubst %.c,%.o,$(wildcard *.c))
dssih_vendor_deps:=
dssih_static_var:=

ifdef dssih_static
  dssih_static_var:=-static
endif

ifdef dssih_system
  dssih_ldlibs:=$(dssih_dynamic_libs) $(dssih_ldlibs)
else
  dssih_ldlibs:=$(dssih_static_libs) $(dssih_ldlibs)
  dssih_cflags:=$(dssih_include_dirs) $(dssih_cflags)
  dssih_vendor_deps:=$(dssih_static_libs)
endif

all: dssih

dssih: $(dssih_vendor_deps) $(dssih_objects)
	$(CC) $(dssih_static_var) $(dssih_cflags) $(dssih_objects) $(dssih_ldflags) $(dssih_ldlibs) -o dssih

$(dssih_objects): %.o: %.c
	$(CC) -c $(dssih_cflags) $< -o $@

$(dssih_vendor_deps):
	cd vendor/soundio && mkdir -p build && cd build && cmake .. -DENABLE_COREAUDIO:BOOL=OFF -DENABLE_JACK:BOOL=OFF -DENABLE_PULSEAUDIO:BOOL=ON -DENABLE_WASAPI:BOOL=OFF && make

install: dssih
	install -D -v -m 755 dssih $(DESTDIR)$(prefix)/bin/dssih

clean:
	rm -f dssih $(dssih_objects) $(dssih_vendor_deps)
	command -v git && git submodule foreach git clean -fdx || true

.PHONY: all install clean
