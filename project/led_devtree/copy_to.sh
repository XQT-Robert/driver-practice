#!/bin/bash

# 本脚本用于复制当前文件，并修改其中 Makefile 以及相关的文件名

# 判断是否有参数传递进来
if [ -z "$1" ]; then
    echo "请提供新的文件名"
    exit 1
fi

#-e等文件判断符号只能判断单个文件
if [  -z "$(find . -name '*.ko')" ]; then
	echo "还没有构造模块，现在构建，quick_mod_test"
	./quick_mod_test.sh
fi

# 获取当前项目名称
old_project_name=$(find . -name "*.ko" | sed 's|^\./||' | sed 's|\.ko$||')


# 新的项目名称，从参数中获取
new_project_name=$1

echo $old_project_name
echo $new_project_name


# 创建新的项目目录
mkdir -p ../$new_project_name

# 检查并复制源文件
if [ -f "${old_project_name}.c" ]; then
    cp ${old_project_name}.c ../$new_project_name/${new_project_name}.c
else
    echo "源文件 ${old_project_name}.c 不存在"
    exit 1
fi

if [ -f "${old_project_name}_App.c" ]; then
    cp ${old_project_name}_App.c ../$new_project_name/${new_project_name}_App.c
else
    echo "源文件 ${old_project_name}_App.c 不存在"
fi

# 复制 Makefile
cp Makefile ../$new_project_name/

# 修改 Makefile 中的目标文件名
sed -i "s/${old_project_name}.o/${new_project_name}.o/g" ../$new_project_name/Makefile

cp copy_to.sh ../$new_project_name/
cp quick_mod_test.sh ../$new_project_name/
cp -r .vscode ../$new_project_name/
cp ${old_project_name}.code-workspace ../$new_project_name/${new_project_name}.code-workspace

echo "项目 ${new_project_name} 已成功创建"
