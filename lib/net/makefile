TARGET = netfscc

SOURCES = src\Fscc.cs
CFC_DIR = ..\c
LIBS = $(CFC_DIR)\cfscc.lib
LIBSD = $(CFC_DIR)\cfsccd.lib

all: $(TARGET).dll 
debug: $(TARGET)d.dll

$(TARGET).dll:$(SOURCES) $(LIBS)
  csc /linkresource:$(CFC_DIR)\cfscc.dll /t:library /out:$@ $(SOURCES) /platform:x86  

$(TARGET)d.dll:$(SOURCES) $(LIBSD)
  csc /linkresource:$(CFC_DIR)\cfsccd.dll /t:library /out:$@ $(SOURCES) /platform:x86
  
$(LIBS):
  pushd $(CFC_DIR) & nmake & popd
  
$(LIBSD):
  pushd $(CFC_DIR) & nmake DEBUG & popd
  
clean:
  del netfscc*