import os
import shutil
import re

# 输入文件路径和输出目录
md_file_path = './.md'
output_dir = '../images/'

# 当前图片的索引
index = 87

# 创建输出目录
os.makedirs(output_dir, exist_ok=True)

# 读取 Markdown 文件
with open(md_file_path, 'r', encoding='utf-8') as file:
    content = file.read()

# 匹配图片格式 ![image-xxxx](C:/path/to/image.png)
pattern = r'!\[.*?\]\((C:/[^)]+)\)'

# 匹配所有图片路径
matches = re.findall(pattern, content)

# 复制图片到输出目录，并更新Markdown文件中的链接
for i, image_path in enumerate(matches, start=1):
    i += index
    # 生成新的文件名，假设是 jpg 格式
    new_filename = f"{i}.jpg"
    
    # 构建新的图片路径
    new_image_path = os.path.join(output_dir, new_filename)
    
    # 复制图片到输出目录
    shutil.copyfile(image_path, new_image_path)
    
    # 更新Markdown文件中的链接
    content = content.replace(f"![]({image_path})", f"![]({output_dir}{new_filename})")

# 将更新后的内容写回Markdown文件
with open(md_file_path, 'w', encoding='utf-8') as file:
    file.write(content)

