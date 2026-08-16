ROOT := $(CURDIR)
TI_GENERATED := $(ROOT)/src/generated
CLANG_FORMAT ?= clang-format

.PHONY: gendb gendb-test

gendb:
	python3 ./tidbgen/main.py --sig-dir ../../sig/micropython --out $(TI_GENERATED)
	$(CLANG_FORMAT) -i $(TI_GENERATED)/micropython_ti_builtin_database.c \
		$(TI_GENERATED)/micropython_ti_builtin_database.h

gendb-test: gendb
	$(MAKE) -C ./host_test test
