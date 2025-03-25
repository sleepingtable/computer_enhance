sim8086:
	clang++ -std=c++23 disassembly.cpp -o disassembly

clean:
	rm disassembly

.PHONY: tests

tests:
	for n in 37 38 39 40 41 ; do \
		./disassembly tests/listing_00$$n > tests/test_listing00$$n.asm ; \
		nasm tests/test_listing00$$n.asm || (echo "failed test listing 00$$n"; exit 1) ; \
		diff tests/test_listing00$$n tests/listing_00$$n || (echo "failed test listing 00$$n"; exit 1) ; \
	done
	@echo "passed all tests!"
