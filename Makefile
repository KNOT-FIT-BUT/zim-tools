# Makefile

OPENZIM_DIR=$(HOME)/openzim
LIBZIM_DIR=$(OPENZIM_DIR)/libzim

CPATH=$(LIBZIM_DIR)/include
LIBRARY_PATH=$(LIBZIM_DIR)/build/src

meson=$(HOME)/.local/bin/meson

.PHONY: all libzim run

all: $(meson) libzim
	CPATH=$(CPATH) LIBRARY_PATH=$(LIBRARY_PATH) $(meson) . build
	CPATH=$(CPATH) LIBRARY_PATH=$(LIBRARY_PATH) ninja -C build

libzim:
	make -C $(LIBZIM_DIR)

$(meson):
	pip3 install --user meson==0.43.0

run:
	echo LD_LIBRARY_PATH=$(LIBRARY_PATH) $(OPENZIM_DIR)/zim-tools/build/src/zimdump -a html_from_wikipedia_en_all_novid_2018-10.zim/ -J en wikipedia_en_all_novid_2018-10.zim

# EOF
