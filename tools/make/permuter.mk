PERMUTE         ?= $(VPYTHON) tools/decomp-permuter/permuter.py
IMPORT          ?= $(VPYTHON) tools/decomp-permuter/import.py
PERMUTEFLAGS    ?= -j8
PERMUTER_SOURCE ?= $(patsubst $(BUILD)/%/,%.c,$(dir $(subst nonmatchings/,,$(TARGET))))

FDIFF ?= $(VPYTHON) tools/dev/fdiff.py

PYTHONDEPS += toml
SKIPSPLAT  += permute decompme fdiff

.PHONY: permute decompme fdiff

permute: $(patsubst %.s,nonmatchings/%/,$(notdir $(TARGET)))
	$(PERMUTE) $(PERMUTEFLAGS) $<

decompme: IMPORTFLAGS += --decompme --preserve-macros="NULL"
decompme: $(PERMUTER_SOURCE) $(TARGET)
	$(IMPORT) $(IMPORTFLAGS) $^

nonmatchings/%/: $(PERMUTER_SOURCE) $(TARGET)
	$(IMPORT) $(IMPORTFLAGS) $^

# Normalized single-function diff: rebuilds the overlay, then shows our code
# against the pristine original with addresses collapsed to relative offsets so
# only real differences survive. Run it inside the dev container.
#
#   make fdiff MOD=PLG085.BIN FN=func_800F9818
#   make fdiff MOD=PLG085.BIN FN=func_800F9818 FLAGS="-n 20 -c 4"
#   make fdiff MOD=PLG085.BIN FN=func_800F9818 FLAGS=--raw
fdiff:
	$(FDIFF) $(MOD) $(FN) $(FLAGS)
