TARGET = tutorial

SOURCES = tutorial.c
CFC_DIR = ..\..\lib\raw\

all: $(TARGET).exe

$(TARGET).exe:$(SOURCES)
  cl /D_WIN32_WINNT=0x0502 /W4 /MT $(SOURCES) /I $(CFC_DIR) /Fe$@
  
clean:
  del *.exe