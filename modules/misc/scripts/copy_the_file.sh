#!/bin/bash

#
# @name         ：快速复制模块脚本
# @Usages       ：./copy_the_file.sh [新建的模块名]
# @description  ：
#   本脚本用于复制当前模块文件到文件同路径下，并修改其中 Makefile 以及相关的文件名
# @configuration：
#	无须配置
# @notice  		：
#   1、修改相对路径应基于控制脚本ez_control的位置，并在控制脚本所处路径下访问该脚本。
#   2、本脚本只会在移动中修改Makefile的默认模块名，如果有多个模块编译注意手动修改，
#      方法如下：objs + = xx.o，注意不要使用:=，赋值会覆盖上面的默认模块而不是添加
#   3、脚本仅复制单个模块，默认选择和App同名
#---

# 判断是否有参数传递进来
if [ -z "$1" ]; then
    echo "请提供新的文件名"
    exit 1
fi

#-e等符号只能判断单个文件
if [  -z "$(find . -name '*.ko')" ]; then
	echo "还没有构造测试过模块，模块可能不可用"
    echo "现在尝试构建，make_and_mount"
	./script/make_and_mount.sh
fi

# 获取当前项目名称
current_project_name=$(find . -name "*_App*" | head -n 1 | \
                                       xargs -n 1 basename -s .c |\
                                       sed 's/_App$//'
                                       )


# 新的项目名称，从参数中获取
new_project_name=$1

echo "current file name :$current_project_name"
echo "new file name : $new_project_name"

# 创建新的项目目录
mkdir -p ../$new_project_name

#移动测试程序
if [ -f "${current_project_name}_App.c" ]; then
    cp ${current_project_name}_App.c ../$new_project_name/${new_project_name}_App.c
else
    echo "源文件 ${current_project_name}_App.c 不存在"
fi

# 检查并复制源文件
if [ -f "${current_project_name}.c" ]; then
    cp ${current_project_name}.c ../$new_project_name/${new_project_name}.c
else
    echo "源文件 ${current_project_name}.c 不存在"
    exit 1
fi

# 复制 Makefile
cp Makefile ../$new_project_name/

# 修改 Makefile 中的目标文件名
sed -i "s/${current_project_name}.o/${new_project_name}.o/g" ../$new_project_name/Makefile
echo "Makefile仅修改添加默认的一个模块文件，注意修改！"

cp -r scripts ../$new_project_name/
cp ez_control.sh ../$new_project_name/
cp -r .vscode ../$new_project_name/
cp ${current_project_name}.code-workspace ../$new_project_name/${new_project_name}.code-workspace

echo "项目 ${new_project_name} 已成功创建"
