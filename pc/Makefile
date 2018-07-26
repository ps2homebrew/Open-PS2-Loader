all:
ifeq ($(_WIN32),1)
	make _WIN32=1 -C iso2opl
	make _WIN32=1 -C opl2iso
	make _WIN32=1 -C genvmc
else
	make -C iso2opl
	make -C opl2iso
	make -C genvmc
endif

clean:
	make -C iso2opl clean
	make -C opl2iso clean
	make -C genvmc clean

rebuild: clean all
