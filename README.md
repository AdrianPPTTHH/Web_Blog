打开端口（设置防火墙）

sudo iptables -L

sudo iptables -A INPUT -p tcp --dport 8876 -j ACCEPT