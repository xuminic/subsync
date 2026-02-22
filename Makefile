
ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif


ifeq ($(OS), Windows_NT)
MINGWDEPS = $(LIBICONV)
MINGWFLAG = -I./$(LIBICONV)/include -L./$(LIBICONV)/lib/.libs
MINGWLIBS = -liconv
endif

LIBICONV = libiconv-1.18
TARGET  = subsync
SOURCE	= subsync.c utf.c
VERSION = 1.0.0
CFLAGS	= -Wall -O3 -DVERSION=\"$(VERSION)\" -DCFG_LIBICONV #-DDEBUG

ICONV_W32   = -I./$(LIBICONV)_i686/include -L./$(LIBICONV)_i686/lib/.libs
ICONV_W64   = -I./$(LIBICONV)_x86_64/include -L./$(LIBICONV)_x86_64/lib/.libs

all: $(TARGET)

# For cross-building Win32 and Win64 targets in Linux
allwin: $(TARGET) $(TARGET)_i686.exe $(TARGET)_x86_64.exe

$(TARGET): $(MINGWDEPS) $(SOURCE)
	gcc $(CFLAGS) $(MINGWFLAG) -o $@ $(SOURCE) $(MINGWLIBS)
	ldd $(TARGET)

$(TARGET)_i686.exe:  $(LIBICONV)_i686 $(SOURCE)
	i686-w64-mingw32-gcc $(CFLAGS) $(ICONV_W32) -o $@ $(SOURCE) -liconv
	i686-w64-mingw32-objdump -p $@ | grep "DLL Name"

$(TARGET)_x86_64.exe: $(LIBICONV)_x86_64 $(SOURCE)
	x86_64-w64-mingw32-gcc $(CFLAGS) $(ICONV_W64) -o $@ $(SOURCE) -liconv
	x86_64-w64-mingw32-objdump -p $@ | grep "DLL Name"

clean:
	rm -f $(TARGET) $(TARGET).exe
	rm -f $(TARGET)_i686.exe $(TARGET)_x86_64.exe

cleanall: clean
	rm -rf $(LIBICONV)_i686 $(LIBICONV)_x86_64

utf: utf.c
	gcc $(CFLAGS) -DUTF_MAIN -o $@ $^

install: $(TARGET)
	install -s $(TARGET) $(PREFIX)/bin
	install -d $(PREFIX)/share/man/man1
	install -m 644 $(TARGET).1 $(PREFIX)/share/man/man1

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET) $(PREFIX)/share/man/man1/$(TARGET).1

$(TARGET).pdf: $(TARGET).1
	man -l -Tps $< | ps2pdf - $@

release: release-src release-win

release-src:
	mkdir $(TARGET)-$(VERSION)
	cp LICENSE Makefile README* subsync.1 subsync.c $(TARGET)-$(VERSION)
	tar czf $(TARGET)-$(VERSION).tar.gz $(TARGET)-$(VERSION)
	rm -rf $(TARGET)-$(VERSION)

release-win: $(TARGET)_i686.exe $(TARGET)_x86_64.exe $(TARGET).pdf
	mkdir $(TARGET)-$(VERSION)-win
	cp LICENSE README* $(TARGET).pdf $(TARGET)_*.exe $(TARGET)-$(VERSION)-win
	zip -r $(TARGET)-$(VERSION)-win.zip $(TARGET)-$(VERSION)-win
	rm -rf $(TARGET)-$(VERSION)-win

release-clean:
	rm -f $(TARGET)-$(VERSION).tar.gz $(TARGET)-$(VERSION)-win.zip


$(LIBICONV)_i686: $(LIBICONV).tar.gz
	tar zxf $(LIBICONV).tar.gz
	mv $(LIBICONV) $(LIBICONV)_i686
	(cd $(LIBICONV)_i686; ./configure --host=i686-w64-mingw32 --enable-static --disable-shared; make)

$(LIBICONV)_x86_64: $(LIBICONV).tar.gz
	tar zxf $(LIBICONV).tar.gz
	mv $(LIBICONV) $(LIBICONV)_x86_64
	(cd $(LIBICONV)_x86_64; ./configure --host=x86_64-w64-mingw32 --enable-static --disable-shared; make)

$(LIBICONV): $(LIBICONV).tar.gz
	tar zxf $(LIBICONV).tar.gz
	(cd $(LIBICONV); ./configure --enable-static --disable-shared; make)

$(LIBICONV).tar.gz:
	# somehow wget is broken in MSYS. Using curl instead
	#wget https://ftp.gnu.org/pub/gnu/libiconv/$(LIBICONV).tar.gz
	curl -o $@ https://ftp.gnu.org/pub/gnu/libiconv/$(LIBICONV).tar.gz


