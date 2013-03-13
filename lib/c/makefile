TARGET = cfscc

SOURCES = src/fscc.c src/calculate-clock-bits.c
HEADERS = src/fscc.h

all: $(TARGET).dll
debug: $(TARGET)d.dll

$(TARGET).dll:$(SOURCES) $(HEADERS)
  cl /W4 /WX /D_WIN32_WINNT=0x0502 $(SOURCES) /Fe$@ /LD

$(TARGET)d.dll:$(SOURCES) $(HEADERS)
  cl /W4 /WX /D_WIN32_WINNT=0x0502 $(SOURCES) /Fe$@ /LDd /Zi
  
clean:
  del cfscc* *.obj