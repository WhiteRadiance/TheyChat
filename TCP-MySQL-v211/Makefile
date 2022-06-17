# Makefile 
# [简述]:
# 	编译并生成 server 和 client 两个可执行文件 
# 	默认清理所有中间文件(*.o)
# 	make clean命令被改为:仅删除可执行文件 
# [语法]:
# 	target: INCends			表示目标依赖关系,第一个目标为make的默认目标 
# 	<TAB>command			表示shell命令,前面必须有一个制表符 
# 	<TAB>@command			表示不要在终端显示这个命令(仅执行命令)
# 	.PHONY: target			表示伪目标,无视依赖的新旧强制执行命令,且无新目标输出 
# [自动变量]:
# 	$@	=	目标集合,即在冒号左边的target 
# 	$<	=	第一个依赖文件,即在冒号右边的第一个文件(集合)
# 	$^	=	所有的依赖文件,会去除重复的依赖文件 
# 	$*	=	不包含扩展名的目标文件名称 
#
#
# CPPFLAGS: C/C++通用的编译参数
# CXXFLAGS: C++专用的编译选项
# -O0~3  指定编译器的优化等级,O0代表不优化(默认)
# -Wall  即show all W(arnings),显示所有警告,便于查错
# -I ./  指定额外的头文件路径,默认为/usr/include和/usr/local/include
# -L ./  指定额外的动态库路径,除非声明'-static',默认为/usr/lib和/usr/local/lib

CXXFLAGS := -std=c++17
# CPPFLAGS := -Wall -O0 -I ./
# CPPFLAGS := -O1
CPPFLAGS := 

EXE1 := server
EXE2 := client

OBJ1 := server_tcp_mature_v2.o \
		mainDef.o \
		IO_api.o \
		CtrlPanel.o \
		Database.o \
		ThreadComm.o
DLL1 := -lpthread -lmysqlclient

OBJ2 := client_tcp_mature_v2.o
DLL2 := 



all: server client


server: $(OBJ1)
	@$(CXX) $(OBJ1) -o $(EXE1) $(DLL1)
	@echo "LD \t $(OBJ1) --> $(EXE1)"

client: $(OBJ2)
	@$(CXX) $(OBJ2) -o $(EXE2) $(DLL2)
	@echo "LD \t $(OBJ2) --> $(EXE2)"



.PHONY: clean  info
clean:
	@rm $(OBJ1) $(OBJ2)
	@echo "CLR\t $(OBJ1) $(OBJ2)"


info:
	@echo "CC=$(CC), CPPFLAG=$(CPPFLAGS), CFLAGS=$(CFLAGS)"
	@echo "CXX=$(CXX), CPPFLAG=$(CPPFLAGS), CXXFLAGS=$(CXXFLAGS)"



# 隐晦规则: "$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@", 加上各自依赖的头文件 

server_tcp_mature_v2.o: mainDef.h  IO_api.h  CtrlPanel.h  Database.h  ThreadComm.h

mainDef.o: mainDef.h
IO_api.o: IO_api.h  CtrlPanel.h  mainDef.h  Database.h
CtrlPanel.o: CtrlPanel.h  mainDef.h
Database.o: Database.h  mainDef.h
ThreadComm.o: ThreadComm.h  mainDef.h  IO_api.h


client_tcp_mature_v2.o: 

# End of Makefile 
