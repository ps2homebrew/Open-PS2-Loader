all:
	ifeq ($(_WIN32),1)
		$(MAKE) _WIN32=1 -C iso2opl
		$(MAKE) _WIN32=1 -C opl2iso
		$(MAKE) _WIN32=1 -C genvmc
	else
		$(MAKE) -C iso2opl
		$(MAKE) -C opl2iso
		$(MAKE) -C genvmc
	endif

clean:
	$(MAKE) -C iso2opl clean
	$(MAKE) -C opl2iso clean
	$(MAKE) -C genvmc clean

rebuild: clean all
