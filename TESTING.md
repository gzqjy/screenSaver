# 屏保与锁屏机制测试说明 (Testing Instructions)

在开发和调试自定义屏保（`ScreenSaver`）时，需要验证它是否能够正确覆盖系统的原生屏保和锁屏界面。
为了方便测试，你可以使用以下命令手动触发系统的各种状态，而无需干等空闲时间。

## 1. 触发系统原生屏保

系统的普通屏保（不一定是锁屏，可能是屏幕变暗或显示动画）。
使用 D-Bus 命令直接调用 UKUI 屏保接口：

```bash
dbus-send --session --dest=org.ukui.ScreenSaver --type=method_call / org.ukui.ScreenSaver.ShowScreensaver
```

## 2. 触发系统锁屏 (UKUI 原生命令)

锁屏界面（需要输入密码解锁）的优先级通常最高，它是我们重点测试“寄生覆盖”逻辑（Reparent）的对象。

```bash
ukui-screensaver-command -l
```

## 3. 使用 Systemd (loginctl) 触发锁屏

除了使用桌面环境自己的命令，还可以通过更底层的 `systemd` 会话管理器来触发锁屏，这对排查底层会话状态很有帮助。

### 第一步：列出当前所有会话
```bash
loginctl list-sessions
```
*你会看到类似如下输出：*
```text
SESSION  UID USER SEAT  TTY
    114 1000 test          
    115 1000 test          
    120 1000 test seat0    
```
*(注意找到包含 `seat0` 或你的主用户的 SESSION ID，例如 `120`)*

### 第二步：查看会话详细信息
确认找到的会话是活跃的图形界面会话：
```bash
loginctl session-status 120
# 或者使用：
# loginctl show-session 120
```
**重点关注以下几项指标：**
- `Type=x11` (或者 wayland) - 代表这是图形界面会话
- `State=active` - 代表该会话当前处于激活状态
- `Remote=no` - 代表是本地直接登录，不是 SSH 远程
- `Class=user` - 代表是用户会话

### 第三步：强制锁定该会话
确认会话 ID 无误后，执行锁定：
```bash
loginctl lock-session 120
```
执行后，该会话对应的图形界面将立刻弹出锁屏。

---

## 验证自定义屏保的覆盖效果

1. 手动触发锁屏后（例如 `loginctl lock-session 120`）。
2. 等待设定的自定义屏保超时时间，或者让自定义屏保先出现，再触发锁屏。
3. 观察现象：我们的 `ScreenSaver` 应该会在 1 秒的心跳检测内，自动发现锁屏界面的出现，并瞬间执行 `Reparent` 操作，将自己覆盖在锁屏界面之上。
4. 移动鼠标或按下键盘：`ScreenSaver` 应该立刻隐藏，露出背后的原生锁屏密码框供你输入密码。
