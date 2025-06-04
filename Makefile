# Makefile הראשי – בונה את כל השלבים (part1 עד part6)

PARTS := part1 part2 part3 part4 part5 part6

.PHONY: all clean $(PARTS)

all: $(PARTS)

$(PARTS):
	$(MAKE) -C $@

clean:
	for part in $(PARTS); do \
		$(MAKE) -C $$part clean; \
	done
