
// 从 Cookie 中获取用户名
var cookie_value = document.cookie;

//因为有可能有双Session的情况  所以需要加上(?:[; ]|$)
var match = cookie_value.match(/username:(.*?)(?:[; ]|$)/);

var usernameValue = match ? match[1] : null;

if (usernameValue) {
    var username = usernameValue;

    // 创建分隔符
    var separator = document.createElement('span');
    separator.textContent = ' | ';

    // 创建欢迎消息元素
    var welcomeText = document.createElement('span');
    welcomeText.textContent = '欢迎 ' + username;

    // 插入到 DOM 中的特定位置，例如在视频旁边
    var videoNavItem = document.querySelector('.nav.navbar-nav.navbar-right li:nth-child(3)');
    videoNavItem.appendChild(welcomeText.cloneNode(true));

    // 让 videoNavItem 可以点击
    videoNavItem.lastChild.style.cursor = 'pointer';
    videoNavItem.lastChild.onclick = function() {
        window.location.href = "/welcome";
    };

    // 创建退出按钮
    var logoutButton = document.createElement('span');
    logoutButton.textContent = '退出登录';
    logoutButton.style.cursor = 'pointer';
    logoutButton.onclick = function() {
        // 清除用户名的 Cookie
        document.cookie = "Session_id=; ";

        // 刷新页面或者重定向到登录页面
        window.location.reload(); // 或者 window.location.href = "/login";
    };

    // 插入退出按钮到 DOM 中
    videoNavItem.appendChild(separator);
    videoNavItem.appendChild(logoutButton);

} else {
    console.error('username cookie not found.');
}
