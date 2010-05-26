all:
ifeq ($(_WIN32),1)
	make _WIN32=1 -C iso2opl
	make _WIN32=1 -C opl2iso
else
	make -C iso2opl
	make -C opl2iso
endif

clean:
	make -C iso2opl clean
	make -C opl2iso clean

rebuild: clean all
