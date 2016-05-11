CC = clang
OPT = opt -load ../build/lib/LLVM-ScalarRepl.so -debug -stats -unroll-runtime=1

# Change this to run whatever optimization passes you want
OPTS = -targetlibinfo -tti -no-aa -tbaa -scoped-noalias -assumption-cache-tracker -basicaa -ipsccp -globalopt -deadargelim -domtree -instcombine -simplifycfg -basiccg -prune-eh -inline-cost -inline -functionattrs -domtree -scalarrepl-zmick2 -early-cse -lazy-value-info -jump-threading -correlated-propagation -simplifycfg -domtree -instcombine -tailcallelim -simplifycfg -reassociate -domtree -loops -loop-simplify -lcssa -loop-rotate -licm -loop-unswitch -instcombine -scalar-evolution -loop-simplify -lcssa -indvars -loop-idiom -loop-deletion -loop-unroll -mldst-motion -domtree -memdep -gvn -memdep -memcpyopt -sccp -domtree -bdce -instcombine -lazy-value-info -jump-threading -correlated-propagation -domtree -memdep -dse -loops -loop-simplify -lcssa -licm -adce -simplifycfg -domtree -instcombine -barrier -float2int -domtree -loops -loop-simplify -lcssa -loop-rotate -branch-prob -block-freq -scalar-evolution -loop-accesses -loop-vectorize -instcombine -scalar-evolution -slp-vectorizer -simplifycfg -domtree -instcombine -loops -loop-simplify -lcssa -scalar-evolution -loop-unroll -instcombine -loop-simplify -lcssa -licm -scalar-evolution -alignment-from-assumptions -strip-dead-prototypes -elim-avail-extern -globaldce -constmerge -verify
# OPTS = -scalarrepl-zmick2 -adce

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
