TARGET = meu_compilador
# Removido o main.o daqui:
OBJS = parser.tab.o lex.yy.o codegen.o

# Configurações da LLVM
LLVM_CONFIG = llvm-config
CXXFLAGS = `$(LLVM_CONFIG) --cppflags` -std=c++17
LDFLAGS = `$(LLVM_CONFIG) --ldflags --libs core --system-libs`

all: $(TARGET)

$(TARGET): $(OBJS)
	g++ -o $(TARGET) $(OBJS) $(LDFLAGS)

parser.tab.cpp parser.tab.hpp: parser.y
	bison -d -o parser.tab.cpp parser.y

lex.yy.cpp: lexer.l parser.tab.hpp
	flex -o lex.yy.cpp lexer.l

%.o: %.cpp
	g++ $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o parser.tab.cpp parser.tab.hpp lex.yy.cpp $(TARGET)
