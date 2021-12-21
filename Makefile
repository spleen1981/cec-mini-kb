CSN_OBJS = cec-mini-kb.o
CSN_BIN = cec-mini-kb
LIBS = -ldl
LIBCEC_VERSION_H = libcec/include/version.h
FAKE_VAR:=$(shell sed 's/\@LIBCEC_VERSION_MAJOR\@/6/;s/\@LIBCEC_VERSION_MINOR\@/0/;s/\@LIBCEC_VERSION_PATCH\@/2/' $(LIBCEC_VERSION_H).in > $(LIBCEC_VERSION_H))

all : $(CSN_BIN)

$(CSN_BIN) : $(CSN_OBJS)
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	-rm $(LIBCEC_VERSION_H)
.PHONY : clean
clean :
	-rm $(CSN_BIN) $(CSN_OBJS) $(LIBCEC_VERSION_H)
