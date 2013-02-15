TARGET = test

SOURCES = test.c
CFC_DIR = ..
HEADERS = $(CFC_DIR)\src\fscc.h
LIBS = $(CFC_DIR)\cfscc.lib

all: $(TARGET).exe

$(TARGET).exe:$(SOURCES) $(HEADERS) $(LIBS)
  cl /D_WIN32_WINNT=0x0502 /W4 /MT $(SOURCES) $(LIBS) /Fe$@
  
clean:
  del *.exe