CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
GCOV_FLAGS = --coverage -fprofile-arcs -ftest-coverage

TARGET_SERVER = atom_warehouse
TARGET_CLIENT = atom_supplier

SRCS_MAIN = atom_warehouse.cpp atom_supplier.cpp
SRCS_LIB =
GCOV_KEEP = atom_warehouse.cpp.gcov atom_supplier.cpp.gcov

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(TARGET_SERVER).cpp
	$(CXX) $(CXXFLAGS) $(GCOV_FLAGS) -o $(TARGET_SERVER) $(TARGET_SERVER).cpp

$(TARGET_CLIENT): $(TARGET_CLIENT).cpp
	$(CXX) $(CXXFLAGS) $(GCOV_FLAGS) -o $(TARGET_CLIENT) $(TARGET_CLIENT).cpp

gcov:
	@if [ -z "$(FILE)" ]; then \
		echo "❌ Usage: make gcov FILE=atom_warehouse.gcno"; \
		exit 1; \
	fi
	@if [ ! -f "$(FILE)" ]; then \
		echo "❌ File '$(FILE)' not found."; \
		exit 1; \
	fi
	@BASE=$$(basename $(FILE)); \
	DIR=$$(dirname $(FILE)); \
	GCDA=$${BASE%.gcno}.gcda; \
	if [ ! -f $$DIR/$$GCDA ]; then \
		echo "❌ File '$$DIR/$$GCDA' not found – did you run the binary?"; \
		exit 1; \
	fi; \
	echo "✅ Running gcov on $$BASE in $$DIR..."; \
	cd $$DIR && gcov -b -p $$BASE > ../.gcov_output.txt; \
	cd ..; \
	echo "---- Filtered Coverage Report (only SRCS cpp files) ----"; \
	for src in $(SRCS_LIB) $(SRCS_MAIN); do \
		grep -A4 "File '$$src'" .gcov_output.txt || echo "⚠️  $$src not covered or missing"; \
	done; \
	echo "🧹 Cleaning .gcov files except project sources..."; \
	find . -name "*.gcov" | while read file; do \
		base=$$(basename $$file); \
		case " $(GCOV_KEEP) " in \
			*" $$base "* ) ;; \
			* ) rm -f "$$file";; \
		esac; \
	done; \
	rm -f .gcov_output.txt

clean:
	rm -f *.o *.gcno *.gcda $(TARGET_SERVER) $(TARGET_CLIENT)

clean-all: clean
	rm -f *.gcov