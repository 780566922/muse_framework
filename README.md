# muse_framework（音频轨 fork）

这是 [MuseScore](https://github.com/musescore/MuseScore) 官方 `muse_framework` 仓库的 **fork**，专门为「MuseScore 音频轨」功能提供音频文件解码能力。

## 这个 fork 是干嘛的

MuseScore 主工程通过 git 子模块引用 `muse_framework`，核心框架代码就放在这里。本项目在主工程里加了「原生音频轨」功能（拖入音频成轨、按拍号+BPM 对齐、底部波形面板等），而真正读取音频文件的**解码器就放在这个仓库**里：

- 分支 `feat/audio-file-source` 在官方基础上新增了解码支持：**WAV / MP3 / Ogg / FLAC / M4A(AAC)**。
- 主工程 `780566922/MuseScore` 的 `feat/audio-track` 分支通过子模块指针，锁定本仓库的提交 `0978eb6`。

## 配合的主工程

| 仓库 | 分支 | 说明 |
| --- | --- | --- |
| [780566922/MuseScore](https://github.com/780566922/MuseScore) | `feat/audio-track` | 音频轨功能主工程（已发布 arm64 DMG） |
| 本仓库 `780566922/muse_framework` | `feat/audio-file-source` | 音频解码子模块 |

## 从源码编译

```bash
git clone --recurse-submodules -b feat/audio-track https://github.com/780566922/MuseScore.git
```

克隆时会自动把本仓库作为 `muse/` 子模块拉下来。完整编译步骤见主工程的说明文档。

## 与上游的关系

- 本仓库的 `main` 分支跟随官方 `musescore/muse_framework` 的 `main`。
- 音频相关的改动**只**集中在 `feat/audio-file-source` 分支；请勿把音频解码代码合并进 `main`，否则会与上游脱节、也无法被主工程引用。
