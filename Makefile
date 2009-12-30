WAF=python tools/waf-light --jobs=1

all:
	@$(WAF) build

all-debug:
	@$(WAF) -v build

all-progress:
	@$(WAF) -p build

install:
	@$(WAF) install

uninstall:
	@$(WAF) uninstall

test: all
	python tools/test.py --mode=release

test-all: all
	python tools/test.py --mode=debug,release

test-debug: all
	python tools/test.py --mode=debug

benchmark: all
	build/default/node benchmark/run.js

doc: doc/node.1 doc/api.html doc/index.html

doc/api.html: doc/api.txt
	asciidoc --unsafe              \
		-a theme=pipe                \
		-a toc                       \
		-a linkcss                   \
		-o doc/api.html doc/api.txt

doc/api.xml: doc/api.txt
	asciidoc -b docbook -d manpage -o doc/api.xml doc/api.txt

doc/node.1: doc/api.xml
	xsltproc --output doc/node.1 --nonet doc/manpage.xsl doc/api.xml

website-upload: doc
	scp doc/* linode:~/tinyclouds/node/

docclean:
	@-rm -f doc/node.1 doc/api.xml doc/api.html

clean: docclean
	@$(WAF) clean

distclean: docclean
	@-rm -rf build/
	@-find tools/ -name "*.pyc" -delete

check:
	@tools/waf-light check

VERSION=$(shell git-describe)
TARNAME=node-$(VERSION)

dist: doc/node.1 doc/api.html
	git-archive --prefix=$(TARNAME)/ HEAD > $(TARNAME).tar
	mkdir -p $(TARNAME)/doc
	cp doc/node.1 $(TARNAME)/doc/node.1
	cp doc/api.html $(TARNAME)/doc/api.html
	tar rf $(TARNAME).tar   \
		$(TARNAME)/doc/node.1 \
		$(TARNAME)/doc/api.html
	rm -r $(TARNAME)
	gzip -f -9 $(TARNAME).tar

.PHONY: benchmark clean docclean dist distclean check uninstall install all test test-all website-upload
