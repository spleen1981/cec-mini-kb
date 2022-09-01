CSN_OBJS = cec-mini-kb.o
CSN_BIN = cec-mini-kb
LIBS = -ldl

all : $(CSN_BIN)

$(CSN_BIN) : $(CSN_OBJS)
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY : clean
clean :
	-rm $(CSN_BIN) $(CSN_OBJS)
