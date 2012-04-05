gcc -fPIC -c liblockdep.c
ld -shared -soname liblockdep.so.1 -o liblockdep.so.1.0 -lc liblockdep.o
ldconfig -v -n .
ln -sf liblockdep.so.1 liblockdep.so
