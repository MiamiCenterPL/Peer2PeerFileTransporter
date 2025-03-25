CC = g++
WXWIDGETS_DIR = wxWidgets-3.2.2.1
WXWIDGETS_URL = https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.2.1/wxWidgets-3.2.2.1.tar.bz2
WXWIDGETS_ARCHIVE = wxWidgets.tar.bz2

SRCS = WxWidgetsGUI.cpp P2P.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = P2PFileTransporter.exe

# Get all .a files and convert to -l format using Windows commands
WXLIBS := $(shell for /F "tokens=*" %%i in ('dir /b "$(WXWIDGETS_DIR)\lib\gcc_lib\*.a"') do @( \
	echo %%i | findstr /r "^lib.*\.a$$" | findstr /v /i "regex scintilla" \
))
WXLIBS := $(WXLIBS:lib%=%)
WXLIBS := $(WXLIBS:.a=)
WXLIBS_FLAGS := $(addprefix -l,$(WXLIBS))

# Core libraries - switch to release versions (remove 'd' suffix)
CORE_LIBS = -lwxmsw32u_core -lwxbase32u

# PNG libs - switch to release versions (remove 'd' suffix)
PNG_LIBS = -lwxpng -lwxzlib

# System library paths
SYS_LIB_PATH = -L"C:/msys64/mingw64/lib"

# Update Windows system libraries to include oleacc
WIN_LIBS = -lcomctl32 -lole32 -lgdi32 -lcomdlg32 -lwinspool -lwinmm -lshell32 -lshlwapi -lversion -loleaut32 -luuid -luxtheme -loleacc

# Update flags for size optimization and release build
CFLAGS = -Wall -O2 -s -DNDEBUG -I./$(WXWIDGETS_DIR)/include -I./$(WXWIDGETS_DIR)/lib/gcc_lib/mswu/
LDFLAGS = -mwindows -static \
		  -L./$(WXWIDGETS_DIR)/lib/gcc_lib \
		  $(SYS_LIB_PATH) \
		  $(CORE_LIBS) \
		  $(WXLIBS_FLAGS) \
		  $(PNG_LIBS) \
		  -lws2_32 \
		  $(WIN_LIBS) \
		  -s

.PHONY: all clean download_wxwidgets build_wxwidgets

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)

$(OBJS): build_wxwidgets

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

download_wxwidgets:
	@if not exist "$(WXWIDGETS_DIR)" ( \
		echo Downloading wxWidgets... && \
		curl -L -o $(WXWIDGETS_ARCHIVE) $(WXWIDGETS_URL) && \
		echo Extracting wxWidgets... && \
		tar -xjf $(WXWIDGETS_ARCHIVE) && \
		del $(WXWIDGETS_ARCHIVE) \
	)

# Update wxWidgets build to use release configuration
build_wxwidgets: download_wxwidgets
	@cd ./$(WXWIDGETS_DIR)/build/msw && \
	mingw32-make -f makefile.gcc SHARED=0 UNICODE=1 BUILD=release RUNTIME_LIBS=static USE_PNG=1

clean:
	del /Q $(OBJS) $(TARGET)
	cd ./$(WXWIDGETS_DIR)/build/msw && mingw32-make -f makefile.gcc clean
