TARGET = cfscc

SOURCES = src/fscc.c src/calculate-clock-bits.c
HEADERS = src/fscc.h

all: $(TARGET).dll
debug: $(TARGET)d.dll

$(TARGET).dll:$(SOURCES) $(HEADERS)
  cl $(SOURCES) /Fe$@ /LD

$(TARGET)d.dll:$(SOURCES) $(HEADERS)
  cl $(SOURCES) /Fe$@ /LDd
  
clean:
  del cfscc* *.obj