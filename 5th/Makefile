MPICC ?= mpicc
MPIEXEC ?= mpirun
CFLAGS ?= -O3 -std=c11 -Wall -Wextra -Wpedantic
LDLIBS ?= -lm

PROGRAMS := sieve_book sieve_odd sieve_local sieve_blocked
MPIEXEC_FLAGS ?= --oversubscribe
ifeq ($(shell id -u),0)
MPIEXEC_FLAGS += --allow-run-as-root
endif

.PHONY: all clean check bench plot

all: $(PROGRAMS)

sieve_book: sieve_book.c sieve_common.h
	$(MPICC) $(CFLAGS) $< -o $@ $(LDLIBS)

sieve_odd: sieve_odd.c sieve_common.h
	$(MPICC) $(CFLAGS) $< -o $@ $(LDLIBS)

sieve_local: sieve_local.c sieve_common.h base_primes.h
	$(MPICC) $(CFLAGS) $< -o $@ $(LDLIBS)

sieve_blocked: sieve_blocked.c sieve_common.h base_primes.h
	$(MPICC) $(CFLAGS) $< -o $@ $(LDLIBS)

check: all
	@set -eu; \
	for program in $(PROGRAMS); do \
	  for np in 1 2 4; do \
	    for test in '2 1' '3 2' '10 4' '100 25' '10000 1229' '1000000 78498'; do \
	      set -- $$test; n=$$1; expected=$$2; args=$$n; \
	      if [ "$$program" = sieve_blocked ]; then args="$$n 32"; fi; \
	      actual=$$($(MPIEXEC) $(MPIEXEC_FLAGS) -np $$np ./$$program $$args | awk '$$1 == "PRIMES" {print $$2}'); \
	      if [ "$$actual" != "$$expected" ]; then \
	        echo "FAIL $$program np=$$np n=$$n expected=$$expected actual=$$actual" >&2; exit 1; \
	      fi; \
	    done; \
	    echo "PASS $$program np=$$np"; \
	  done; \
done
	@echo "All correctness checks passed."

bench: all
	python3 scripts/benchmark.py --n $${N:-10000000} --processes "$${P:-1 2 4}" --repeats $${R:-3} --block-kib $${BLOCK_KIB:-32}

plot:
	@latest=$$(ls -t results/*.csv 2>/dev/null | head -n 1); \
	if [ -z "$$latest" ]; then echo "No CSV found; run 'make bench' first." >&2; exit 1; fi; \
	python3 scripts/plot.py "$$latest" --output "$${latest%.csv}.png"

clean:
	$(RM) $(PROGRAMS)
