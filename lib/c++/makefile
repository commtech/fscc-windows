TARGET = cppfscc

SOURCES = src/fscc.cpp src/sys_exception.cpp
HEADERS = src/fscc.hpp src/sys_exception.hpp ../c/src/fscc.h
CFC_DIR = ..\c
LIBS = $(CFC_DIR)\cfscc.lib
LIBSD = $(CFC_DIR)\cfsccd.lib

all: $(TARGET).dll 
debug: $(TARGET)d.dll

$(TARGET).dll:$(SOURCES) $(HEADERS) $(LIBS)
  cl /W4 /WX $(SOURCES) /I$(CFC_DIR)\src\ $(LIBS) /EHsc /Fe$@ /LD

$(TARGET)d.dll:$(SOURCES) $(HEADERS) $(LIBSD)
  cl /W4 /WX $(SOURCES) /I$(CFC_DIR)\src\ $(LIBSD) /EHsc /Fe$@ /LDd /Zi
  
$(LIBS):
  pushd $(CFC_DIR) & nmake & popd
  
$(LIBSD):
  pushd $(CFC_DIR) & nmake DEBUG & popd
  
clean:
  del cppfscc* *.obj