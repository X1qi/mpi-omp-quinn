MPICC ?= mpicc
MPIEXEC ?= mpirun
CFLAGS ?= -O3 -std=c11 -Wall -Wextra -Wpedantic

PROGRAMS := floyd_p2p floyd_bcast
MPIEXEC_FLAGS ?= --oversubscribe
ifeq ($(shell id -u),0)
MPIEXEC_FLAGS += --allow-run-as-root
endif

.PHONY: all clean check bench plot

all: $(PROGRAMS)

floyd_p2p: floyd_p2p.c floyd_common.h
	$(MPICC) $(CFLAGS) $< -o $@

floyd_bcast: floyd_bcast.c floyd_common.h
	$(MPICC) $(CFLAGS) $< -o $@

check: all
	@set -eu; \
	for program in $(PROGRAMS); do \
	  for np in 1 2 4; do \
	    for n in 1 4 17 64 101; do \
	      output=$$($(MPIEXEC) $(MPIEXEC_FLAGS) -np $$np ./$$program $$n 7); \
	      correct=$$(printf '%s\n' "$$output" | awk '$$1 == "CORRECT" {print $$2}'); \
	      if [ "$$correct" != 1 ]; then \
	        echo "FAIL $$program np=$$np n=$$n" >&2; printf '%s\n' "$$output" >&2; exit 1; \
	      fi; \
	    done; \
	    echo "PASS $$program np=$$np"; \
	  done; \
done
	@echo "All correctness checks passed."

bench: all
	python3 scripts/benchmark.py --n $${N:-512} --processes "$${P:-1 2 4}" --repeats $${R:-3}

plot:
	@latest=$$(ls -t results/*.csv 2>/dev/null | head -n 1); \
	if [ -z "$$latest" ]; then echo "No CSV found; run 'make bench' first." >&2; exit 1; fi; \
	python3 scripts/plot.py "$$latest" --output "$${latest%.csv}.png"

clean:
	$(RM) $(PROGRAMS)
