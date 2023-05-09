#
# Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.  Oracle designates this
# particular file as subject to the "Classpath" exception as provided
# by Oracle in the LICENSE file that accompanied this code.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#

# This must be the first rule
default:

# Inclusion of this pseudo-target will cause make to execute this file
# serially, regardless of -j. Recursively called makefiles will not be
# affected, however. This is required for correct dependency management.
.NOTPARALLEL:

# The shell code below will be executed on /usr/ccs/bin/make on Solaris, but not in GNU make.
# /usr/ccs/bin/make lacks basically every other flow control mechanism.
TEST_FOR_NON_GNUMAKE:sh=echo You are not using GNU make/gmake, this is a requirement. Check your path. 1>&2 && exit 1

# Assume we have GNU make, but check version.
ifeq ($(strip $(foreach v, 3.81% 3.82% 4.%, $(filter $v, $(MAKE_VERSION)))), )
  $(error This version of GNU Make is too low ($(MAKE_VERSION)). Check your path, or upgrade to 3.81 or newer.)
endif

# Locate this Makefile
ifeq ($(filter /%,$(lastword $(MAKEFILE_LIST))),)
  makefile_path:=$(CURDIR)/$(lastword $(MAKEFILE_LIST))
else
  makefile_path:=$(lastword $(MAKEFILE_LIST))
endif
root_dir:=$(dir $(makefile_path))

#
# 一些不依赖于make入口的全局的辅助性函数. ,这里从分析主流程的原因看.
# 这里包含这个文件,主要有两个目的.
# 1. 下面的call调用: ParseConfAndSpec ,主要是解析我查找 SPEC变量的值.
# 2. 为后面的各种脚本调用提供全局的一些辅助函数. 比如异常退出.
# ... and then we can include our helper functions
# 在 MakeHelpers.gmk文件中定义了如下变量: ParseConfAndSpec
include $(root_dir)/make/MakeHelpers.gmk

# call 基本用法.
# 基本语法: $(call variable,param,param,…)
# 语法说明:

# 这里的variable是定义的递归扩展变量的名字. 比如func1. 此类变量一般使用递归扩展的方式进行定义.
# 比如:func_add = $(shell echo $$(( $(1) + $(2) )) ) , 这里定义了一个能够实现数字加法的函数.
# 参数是在扩展 前面的variable的时候传入的值. 分别依次把param1 ,param2 ... 赋值给临时变量:$(1),$(2) .... 用于表达式的扩展.
# 示例, 这里大概定义了一个加法的函数.可以让 make实现加法的操作.

# func_add = $(shell echo $$(( $(1) + $(2) )) )
# $(warning $(call func_add,1,2))
# 输出:
# makefile:3: 3
# make: *** No targets. Stop.
#
# eval 基本用法
# 定义一个模板文件:
# 文件名: hello.mk
# define hello_target
# hello:;
#		@echo $(1) says hello world
#		@echo current directory is:$$(shell pwd)
# endef
# 定义一个主文件:

# 文件名: makefile

# program := main
# include hello.mk


# $(eval $(call hello_target,$(program)))
# 运行:make输出:

# main says hello world
# current directory is:/root/ccode/template

# 解释说明:

# define结合着eval使用的基本功能是把一段代码原样的插入到makefile的一部分. 就像是直接在这里进行书写的一样. 同时makefile会对插入的内容进行语法解析.可以生成相应的变量/ 目标等内容.就像是直接在这里写的一样.
# eval本身会进行一次扩展. 但是扩展的时候传的变量是在主makefile中定义好的. 无法进行函数化的调用参数传入.
# 为了能够函数化的进行参数传递, 先进行一次call操作. 然后再把结果传递给eval进行处理.
# 注: call的处理实际与eval是一样的. 只是call本身是得到一个变量值. (扩展后的) eval有能力让值变为makefile文件的定义的一部分.
#

$(eval $(call ParseLogLevel))

# ParseConfAndSpec上面的注释已经较好的说明了这个流程, 主要就是去搜索和定位一个默认使用的spec.gmk文件的路径并且把其赋值给SPEC变量的过程.
# 主流程会解析出SPEC变量的值.也就是SPEC文件的路径
# ParseConfAndSpec 的核心目标是找出:spec.gmk文件. 此文件是./configure 命令从:common/autoconf/spec.gmk.in 模板填充变量后得到的.
# 位于目录: build/macosx-x86_64-normal-server-slowdebug/spec.gmk ,里面存储了各种各位的变量与配置. 以供下面所有的makefile编译时使用.
$(eval $(call ParseConfAndSpec))

# Now determine if we have zero, one or several configurations to build.
ifeq ($(SPEC),)
  # Since we got past ParseConfAndSpec, we must be building a global target. Do nothing.
else
  ifeq ($(words $(SPEC)),1)
  	# 主流程会走这里 , 因为一般只会加载一个spec说明文件.
    # /path/to/jdk8u/build/macosx-x86_64-normal-server-slowdebug/spec.gmk
    # We are building a single configuration. This is the normal case. Execute the Main.gmk file.
    # 不管是build目录的Makefile还是源代码根目录的:Makefile, 两者最终都是加载了Main.gmk文件进行运行编译的.
    # 我们看此文件的内容也可以发现里面我们找到了makefile中比较熟悉的各种目标的定义
    include $(root_dir)/make/Main.gmk
  else
    # We are building multiple configurations.
    # First, find out the valid targets
    # Run the makefile with an arbitrary SPEC using -p -q (quiet dry-run and dump rules) to find
    # available PHONY targets. Use this list as valid targets to pass on to the repeated calls.
    all_phony_targets=$(filter-out $(global_targets), $(strip $(shell \
        cd $(root_dir) && $(MAKE) -p -q FRC SPEC=$(firstword $(SPEC)) | \
        grep ^.PHONY: | head -n 1 | cut -d " " -f 2-)))

    $(all_phony_targets):
	@$(foreach spec,$(SPEC),(cd $(root_dir) && $(MAKE) SPEC=$(spec) \
	    $(VERBOSE) VERBOSE=$(VERBOSE) LOG_LEVEL=$(LOG_LEVEL) $@) &&) true

    .PHONY: $(all_phony_targets)

  endif
endif

# Here are "global" targets, i.e. targets that can be executed without specifying a single configuration.
# If you addd more global targets, please update the variable global_targets in MakeHelpers.

help:
	$(info )
	$(info OpenJDK Makefile help)
	$(info =====================)
	$(info )
	$(info Common make targets)
	$(info .  make [default]         # Compile all product in langtools, hotspot, jaxp, jaxws,)
	$(info .                         # corba and jdk)
	$(info .  make all               # Compile everything, all repos and images)
	$(info .  make images            # Create complete j2sdk and j2re images)
	$(info .  make docs              # Create javadocs)
	$(info .  make overlay-images    # Create limited images for sparc 64 bit platforms)
	$(info .  make profiles          # Create complete j2re compact profile images)
	$(info .  make bootcycle-images  # Build images twice, second time with newly build JDK)
	$(info .  make install           # Install the generated images locally)
	$(info .  make clean             # Remove all files generated by make, but not those)
	$(info .                         # generated by configure)
	$(info .  make dist-clean        # Remove all files, including configuration)
	$(info .  make help              # Give some help on using make)
	$(info .  make test              # Run tests, default is "jdk_core langtools_jtreg" (see TEST below))
	$(info )
	$(info Targets for specific components)
	$(info (Component is any of langtools, corba, jaxp, jaxws, hotspot, jdk, nashorn, images, overlay-images, docs or test))
	$(info .  make <component>       # Build <component> and everything it depends on. )
	$(info .  make <component>-only  # Build <component> only, without dependencies. This)
	$(info .                         # is faster but can result in incorrect build results!)
	$(info .  make clean-<component> # Remove files generated by make for <component>)
	$(info )
	$(info Useful make variables)
	$(info .  make CONF=             # Build all configurations (note, assignment is empty))
	$(info .  make CONF=<substring>  # Build the configuration(s) with a name matching)
	$(info .                         # <substring>)
	$(info )
	$(info .  make LOG=<loglevel>    # Change the log level from warn to <loglevel>)
	$(info .                         # Available log levels are:)
	$(info .                         # 'warn' (default), 'info', 'debug' and 'trace')
	$(info .                         # To see executed command lines, use LOG=debug)
	$(info )
	$(info .  make JOBS=<n>          # Run <n> parallel make jobs)
	$(info .                         # Note that -jN does not work as expected!)
	$(info )
	$(info .  make test TEST=<test>  # Only run the given test or tests, e.g.)
	$(info .                         # make test TEST="jdk_lang jdk_net")
	$(info .                         # or)
	$(info .                         # make test TEST="tier1")
	$(info )

.PHONY: help
