# 效果




# 安装与配置

## 打开端口（设置防火墙）

sudo iptables -L

sudo iptables -A INPUT -p tcp --dport 8876 -j ACCEPT


## 安装MYSQL、mysql库（ mysql连接库 #include <mysql/mysql.h> )
sudo apt-get install mysql-server
sudo apt-get install libmysqlclient-dev
sudo apt-get install default-libmysqlclient-dev

sqlconnRAII: 构造即初始化(注意是sql 而不是sql池)


数据库操作：

添加用户:
 //%是通配符 表示所有地址连接webserver都可以 也可以写localhost
CREATE USER 'username'@'%' IDENTIFIED BY 'password';

赋予权限:
GRANT ALL PRIVILEGES ON webserver.* TO 'web_admin'@'%';

刷新:
FLUSH PRIVILEGES;


## 安装redis、hiredis
sudo apt-get update
sudo apt-get install redis-server
sudo apt-get install libhiredis-dev


## 安装cmark
github:https://github.com/commonmark/cmark  
链接：-L "/cmark_path/lib"


# 压力测试