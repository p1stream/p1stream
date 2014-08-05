P1_BUILD=node_modules/.bin/p1-build

build: ${P1_BUILD}
	${P1_BUILD} npm install

clean:
	rm -fr build/ node_modules/ web/bower_components/

${P1_BUILD}:
	npm install p1stream/p1-build

.PHONY: build clean
