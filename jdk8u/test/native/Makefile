.DEFAULT: jvm

CFLAGS+=-g -Wall -O0

# 为什么使用clang的2阶段命令行构建不会生成dSYM目录？
# https://qa.1r1g.com/sf/ask/2260814461/
# https://blog.csdn.net/quentin_d/article/details/122879754

.PHONY: jvm
jvm: jvm.c
	@echo "\nbuild start at $(shell date '+%Y%m%d %H:%M:%S')\n"
	cc -v -xc++ ${CFLAGS} ${cur_dir}/$^ -o ${build_dir}/$@ \
	-I${openjdk_home}/include \
	-I${openjdk_home}/include/darwin \
	-I${openjdk_home}/include/linux \
	-DJDK_PATH=\"${openjdk_home}\" \
	-DCLASS_PATH=\"${java_target_dir}\" \
	-DJAVA_MAIN_CLASS=\"${java_main_class}\" \
	-ldl
	@echo "\nbuild success at $(shell date '+%Y%m%d %H:%M:%S')\n"

.PHONY: clean
clean:
	-rm -rf ${build_dir}/*
	-rm -rf ${java_target_dir}/*
