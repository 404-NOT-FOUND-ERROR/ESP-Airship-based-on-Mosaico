# AirBoat design system

[中文](#中文) · [Roadmap](roadmap.html)

The public roadmap and the Mosaico device UI share a small visual language. It
keeps the build readable on a bench, on a phone, and on the 480 × 480 device
screen.

## Visual tokens

| Token | Value | Use |
|---|---|---|
| Ink | `#101010` | primary text, hardware and safety surfaces |
| Paper | `#F6F6F3` | page and device background |
| Line | `#CBCBC6` | quiet boundaries and separators |
| Orange | `#FF4C01` | active action, current stage and attention |
| Green | `#34694F` | verified state and completed work |
| Muted | `#70706C` | secondary copy and measured context |

Use square or lightly rounded containers, one orange action per view, and a
small number of strong labels. Keep the page hierarchy: current state first,
next action second, engineering detail third.

## Interaction rules

- `ARM`, `E-STOP`, `CLEAR` and `DISARM` are explicit state actions. The UI
  reports the state snapshot; it never implies that a motor is powered.
- Every stage has an entry signal, an action, and an exit record.
- A green mark means a recorded check, not a physical flight result.
- Animations are short and positional: reveal a section, move a selected tab,
  or confirm a copy action. `prefers-reduced-motion` removes transition work.
- The public page supports keyboard stage navigation with arrow keys, Home and
  End. The device scene keeps the same clear labels and zero-output default.

## Layout rules

Use a constrained content width, a 12–16 px base spacing step, and a 4 px
minimum touch target gap. Keep labels inside stable boxes so the longest
translated string does not change the control layout. On small screens, stack
the control chain and let the stage selector wrap into two columns.

## 中文

路线页和 Mosaico 设备界面共用一套小型视觉语言，保证桌面、手机和 480 × 480
屏幕上的信息都容易扫描。

- 黑色用于主文字、安全状态和硬件节点；暖白用于页面背景；橙色只表示当前
  动作或注意事项；绿色表示已有记录的验收结果。
- 每个阶段都写清入口、动作和完成记录。绿色勾选表示已经记录检查，不代表
  已经飞行。
- `ARM`、`E-STOP`、`CLEAR`、`DISARM` 都是明确的状态动作；界面只显示状态
  快照，不把动画当作真实电机输出。
- 动效只用于进入、选择和复制反馈；用户开启“减少动态效果”时自动关闭。
- 路线页支持键盘方向键、Home、End；小屏阶段选择器自动换行。
