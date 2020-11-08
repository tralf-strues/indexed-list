Options = -Wall -Wpedantic -DLIST_DEBUG_MODE

SrcDir = src
BinDir = bin
Intermediates = $(BinDir)/intermediates
LibDir = libs

LIBS = $(LibDir)/log_generator.a
DEPS = $(SrcDir)/list.h $(LibDir)/log_generator.h  

$(BinDir)/test.exe : $(Intermediates)/test.o $(BinDir)/indexed_list.a $(DEPS)
	g++ -o $(BinDir)/test.exe $(Intermediates)/test.o $(BinDir)/indexed_list.a $(LIBS)

$(Intermediates)/test.o : $(SrcDir)/test.cpp $(DEPS)
	g++ -o $(Intermediates)/test.o -c $(SrcDir)/test.cpp $(Options)

$(BinDir)/indexed_list.a : $(Intermediates)/list.o $(DEPS)
	ar ru $(BinDir)/indexed_list.a $(Intermediates)/list.o $(LIBS)
	
$(Intermediates)/list.o : $(SrcDir)/list.cpp $(SrcDir)/list.h $(DEPS)
	g++ -o $(Intermediates)/list.o -c $(SrcDir)/list.cpp $(Options)