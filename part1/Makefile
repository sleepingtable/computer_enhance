sim8086:
	clang++ -std=c++23 -march=native -O3 sim8086.cpp -o sim8086

clean:
	rm sim8086

.PHONY: tests

tests:
	for n in 37 38 39 40 41 ; do \
		./sim8086 tests/listing_00$$n > tests/test_listing00$$n.asm ; \
		nasm tests/test_listing00$$n.asm || (echo "failed test listing 00$$n"; exit 1) ; \
		diff tests/test_listing00$$n tests/listing_00$$n || (echo "failed test listing 00$$n"; exit 1) ; \
	done
	@echo "passed all tests!"
