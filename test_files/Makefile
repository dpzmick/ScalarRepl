CC = clang
OPT = opt -load ../build/lib/LLVM-ScalarRepl.so -debug -stats -unroll-runtime=1

LOOP = -simplifycfg  -loops  -lcssa -loop-simplify -loop-rotate -loop-unroll -unroll-count=3 -unroll-allow-partial
# Change this to run whatever optimization passes you want
OPTS = -scalarrepl-zmick2 -adce

make:
	@make -s -C ../build

# Make a .ll file from a .bc file, using llvm-dis
%.ll: %.bc clean make
	llvm-dis < $< > $@

%.exe: %.bc clean make
	llc -filetype=obj $<
	gcc $(<:%.bc=%.o) -o $@

# Make a .bc file from a .c file by running through llvm-gcc, with
# optimizations turned off, and then running a few basic
# optimizations.
%.bc: %.c clean make
	$(CC) -c -O0 -emit-llvm $< -o - | opt > $@

# Run the optimizations specified in OPTS
%-opt.bc: %.bc clean make
	$(OPT) $(OPTS) < $< > $@

clean :
	rm -f *.bc *.ll *.o *.exe
